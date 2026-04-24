/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: cpu_Switch / cpu_Dispatch for AArch64.
          Follows the x86_64-pc model: save/restore ALL registers via et_RegFrame.
          Overrides the generic stubs in rom/kernel/kernel_cpu.c.
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
