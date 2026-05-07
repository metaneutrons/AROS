/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
 *  Author: Fabian Schmieder

    Desc: cpu_Switch / cpu_Dispatch for AArch64.
          Follows the x86_64-pc model: save/restore ALL registers via et_RegFrame.
          Overrides the generic stubs in rom/kernel/kernel_cpu.c.

    FPU: Lazy context switch — FPU is disabled (CPACR_EL1.FPEN=0) after
    dispatch. First FPU access traps (EC=0x07), handler saves previous
    owner's state and restores current task's state, then re-enables FPU.
    Tasks that never use FPU pay zero save/restore cost.
*/

#include <exec/execbase.h>
#include <exec/alerts.h>
#include <hardware/intbits.h>
#include <aros/aarch64/cpucontext.h>
#include <proto/exec.h>

#include <kernel_base.h>
#include <kernel_scheduler.h>
#include <kernel_syscall.h>
#include "kernel_intern.h"
#include <kernel_intr.h>
#include "tls.h"

#define AROS_NO_ATOMIC_OPERATIONS
#include "exec_platform.h"

/* Per-CPU: last task that used the FPU (owns current HW FPU state) */
static struct Task *fpu_owner;

/*
 * fpu_save — save FPU/NEON registers to task's VFPContext.
 */
static inline void fpu_save(struct VFPContext *vfp)
{
    __asm__ volatile(
        "stp q0,  q1,  [%0, #0x000]\n"
        "stp q2,  q3,  [%0, #0x020]\n"
        "stp q4,  q5,  [%0, #0x040]\n"
        "stp q6,  q7,  [%0, #0x060]\n"
        "stp q8,  q9,  [%0, #0x080]\n"
        "stp q10, q11, [%0, #0x0A0]\n"
        "stp q12, q13, [%0, #0x0C0]\n"
        "stp q14, q15, [%0, #0x0E0]\n"
        "stp q16, q17, [%0, #0x100]\n"
        "stp q18, q19, [%0, #0x120]\n"
        "stp q20, q21, [%0, #0x140]\n"
        "stp q22, q23, [%0, #0x160]\n"
        "stp q24, q25, [%0, #0x180]\n"
        "stp q26, q27, [%0, #0x1A0]\n"
        "stp q28, q29, [%0, #0x1C0]\n"
        "stp q30, q31, [%0, #0x1E0]\n"
        "mrs x1, fpsr\n"
        "mrs x2, fpcr\n"
        "str w1, [%0, #0x200]\n"
        "str w2, [%0, #0x204]\n"
        : : "r"(vfp) : "x1", "x2", "memory"
    );
}

/*
 * fpu_restore — load FPU/NEON registers from task's VFPContext.
 */
static inline void fpu_restore(struct VFPContext *vfp)
{
    __asm__ volatile(
        "ldp q0,  q1,  [%0, #0x000]\n"
        "ldp q2,  q3,  [%0, #0x020]\n"
        "ldp q4,  q5,  [%0, #0x040]\n"
        "ldp q6,  q7,  [%0, #0x060]\n"
        "ldp q8,  q9,  [%0, #0x080]\n"
        "ldp q10, q11, [%0, #0x0A0]\n"
        "ldp q12, q13, [%0, #0x0C0]\n"
        "ldp q14, q15, [%0, #0x0E0]\n"
        "ldp q16, q17, [%0, #0x100]\n"
        "ldp q18, q19, [%0, #0x120]\n"
        "ldp q20, q21, [%0, #0x140]\n"
        "ldp q22, q23, [%0, #0x160]\n"
        "ldp q24, q25, [%0, #0x180]\n"
        "ldp q26, q27, [%0, #0x1A0]\n"
        "ldp q28, q29, [%0, #0x1C0]\n"
        "ldp q30, q31, [%0, #0x1E0]\n"
        "ldr w1, [%0, #0x200]\n"
        "ldr w2, [%0, #0x204]\n"
        "msr fpsr, x1\n"
        "msr fpcr, x2\n"
        : : "r"(vfp) : "x1", "x2", "memory"
    );
}

/*
 * fpu_disable — trap next FPU access (CPACR_EL1.FPEN = 0b00).
 */
static inline void fpu_disable(void)
{
    uint64_t cpacr;
    __asm__ volatile("mrs %0, cpacr_el1" : "=r"(cpacr));
    cpacr &= ~(3UL << 20);
    __asm__ volatile("msr cpacr_el1, %0; isb" : : "r"(cpacr));
}

/*
 * fpu_enable — allow FPU access (CPACR_EL1.FPEN = 0b11).
 */
static inline void fpu_enable(void)
{
    uint64_t cpacr;
    __asm__ volatile("mrs %0, cpacr_el1" : "=r"(cpacr));
    cpacr |= (3UL << 20);
    __asm__ volatile("msr cpacr_el1, %0; isb" : : "r"(cpacr));
}

/*
 * fpu_trap_handler — called when a task accesses FPU while disabled.
 * ESR_EL1 EC=0x07 (FP/SIMD access trap).
 * Saves previous owner's FPU state, restores current task's, enables FPU.
 */
void fpu_trap_handler(void)
{
    struct Task *task = GET_THIS_TASK;
    if (!task) return;

    /* Save previous owner's FPU state */
    if (fpu_owner && fpu_owner != task)
    {
        struct ExceptionContext *octx = fpu_owner->tc_UnionETask.tc_ETask->et_RegFrame;
        if (octx && octx->fpuContext)
        {
            fpu_save((struct VFPContext *)octx->fpuContext);
            octx->Flags |= ECF_FPU;
        }
    }

    /* Restore current task's FPU state (if it has one) */
    if ((task->tc_Flags & TF_ETASK) && task->tc_UnionETask.tc_ETask->et_RegFrame)
    {
        struct ExceptionContext *ctx = task->tc_UnionETask.tc_ETask->et_RegFrame;
        if (ctx->fpuContext && (ctx->Flags & ECF_FPU))
            fpu_restore((struct VFPContext *)ctx->fpuContext);
    }

    fpu_owner = task;
    fpu_enable();
}

/*
 * cpu_Switch — save current task's register state.
 *
 * Called from core_SysCall() and core_ExitInterrupt() via kernel_intr.c.
 * 'regs' points to the ExceptionContext saved on SP_EL1 by the ASM stub.
 * We copy it into the task's et_RegFrame.
 */
void cpu_Switch(regs_t *regs)
{
    struct Task *task = GET_THIS_TASK;

    if (task)
    {
        if ((task->tc_Flags & TF_ETASK) && task->tc_UnionETask.tc_ETask->et_RegFrame)
        {
            struct ExceptionContext *ctx = task->tc_UnionETask.tc_ETask->et_RegFrame;
            /* Copy GPR area: r[0]-r[28], fp, lr, sp, pc, pstate = 34 IPTRs */
            CopyMem(regs, ctx, 34 * sizeof(IPTR));
        }

        task->tc_SPReg = (APTR)regs->sp;
        task->tc_TDNestCnt = TDNESTCOUNT_GET;
        task->tc_IDNestCnt = IDNESTCOUNT_GET;

        /* Reset to -1 for the idle loop / next dispatch */
        TDNESTCOUNT_SET(-1);
        IDNESTCOUNT_SET(-1);

        /*
         * core_Switch is now safe — our arch-specific version checks
         * for NULL node pointers before Remove(). core_Dispatch nulls
         * them after REMHEAD. So we can always call core_Switch.
         */
        core_Switch();

        /*
         * Clear ThisTask AFTER core_Switch() completes. core_Switch()
         * needs GET_THIS_TASK to work. Once it returns, the task is
         * safely on TaskReady/TaskWait. If an IRQ fires after this
         * point and triggers a nested cpu_Switch, ThisTask=NULL
         * prevents overwriting the saved context.
         */
        TLS_SET(ThisTask, NULL);
    }
}

/*
 * cpu_Dispatch — pick next task and load its register state.
 *
 * Called from core_SysCall() and core_ExitInterrupt().
 * Overwrites 'regs' with the new task's saved context.
 */
void cpu_Dispatch(regs_t *regs)
{
    struct Task *task;

    TLS_SET(ThisTask, NULL);

    while (!(task = core_Dispatch()))
    {
        __asm__ volatile("msr daifclr, #2" ::: "memory");
        __asm__ volatile("wfi");
        __asm__ volatile("msr daifset, #2" ::: "memory");
        if (SysBase->SysFlags & SFF_SoftInt)
            core_Cause(INTB_SOFTINT, 1L << INTB_SOFTINT);
        if (FLAG_SCHEDSWITCH_ISSET)
            FLAG_SCHEDSWITCH_CLEAR;
    }

    /*
     * core_Dispatch nulls the task's node pointers after REMHEAD,
     * and core_Switch checks for NULL before Remove(). No need for
     * daifset or manual null-out here.
     */

    TLS_SET(ThisTask, task);
    TDNESTCOUNT_SET(task->tc_TDNestCnt);
    IDNESTCOUNT_SET(task->tc_IDNestCnt);
    FLAG_SCHEDSWITCH_CLEAR;

    if ((task->tc_Flags & TF_ETASK) && task->tc_UnionETask.tc_ETask->et_RegFrame)
    {
        struct ExceptionContext *ctx = task->tc_UnionETask.tc_ETask->et_RegFrame;
        CopyMem(ctx, regs, 34 * sizeof(IPTR));
        /*
         * Clear DAIF.I (bit 7) in the restored PSTATE. Tasks that called
         * Wait() had Disable() active (DAIF.I=1) when they entered the
         * SVC handler. Without clearing it, the task resumes with IRQs
         * masked and can never receive timer or other interrupt-driven
         * signals. The task's IDNestCnt tracks the logical Disable state;
         * Enable() will be called by Wait() after the signal check.
         */
        regs->pstate &= ~(1UL << 7);  /* Clear DAIF.I */
    }

    /*
     * Lazy FPU: disable FPU access for the new task. If it uses FPU,
     * the trap handler (EC=0x07) will save the previous owner's state
     * and restore this task's state on demand.
     * Skip if this task already owns the FPU (common case: same task re-dispatched).
     */
    if (task != fpu_owner)
        fpu_disable();
}

#if defined(__AROSEXEC_SMP__)
#include "kernel_scheduler.h"

void core_InitScheduleData(struct AArch64SchedulerPrivate *schedData)
{
    schedData->Granularity = SCHEDGRAN_VALUE;
    schedData->Quantum = SCHEDQUANTUM_VALUE;
    schedData->Elapsed = 0;
    schedData->IDNestCnt = -1;
    schedData->TDNestCnt = -1;
}
#endif
