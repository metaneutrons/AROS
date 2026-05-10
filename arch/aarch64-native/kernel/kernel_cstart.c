/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: AArch64 kernel startup — parse boot tags, set up memory, create ExecBase.
          Ported from arch/arm-native/kernel/kernel_startup.c.
*/

#include <aros/kernel.h>
#include <aros/symbolsets.h>
#include <aros/aarch64/cpucontext.h>
#include <exec/memory.h>
#include <exec/memheaderext.h>
#include <exec/tasks.h>
#include <exec/alerts.h>
#include <exec/execbase.h>
#include <proto/kernel.h>
#include <proto/exec.h>

#include <strings.h>
#include <string.h>

#include "exec_intern.h"
#include "etask.h"
#include "tlsf.h"

#include "kernel_intern.h"
#include "kernel_debug.h"
#include "kernel_romtags.h"
#include "gic400.h"

/*
 * Patched AllocMem that strips MEMF_CHIP — AArch64 has no chip memory.
 * Follows the same pattern as arch/arm-native/kernel/platform_init.c.
 */
void *(*__chip_AllocMem)();

#define ExecAllocMem(byteSize, requirements) \
    AROS_CALL2(void *, __chip_AllocMem, \
        AROS_LCA(ULONG, byteSize, D0), \
        AROS_LCA(ULONG, requirements, D1), \
        struct ExecBase *, SysBase)

AROS_LH2(APTR, AllocMem,
        AROS_LHA(ULONG, byteSize, D0),
        AROS_LHA(ULONG, requirements, D1),
        struct ExecBase *, SysBase, 33, Kernel)
{
    AROS_LIBFUNC_INIT
    if (requirements & MEMF_CHIP)
        requirements &= ~MEMF_CHIP;
    return ExecAllocMem(byteSize, requirements);
    AROS_LIBFUNC_EXIT
}

#undef KernelBase
#include "tls.h"
#include "kernel_scheduler.h"

/* BCM2711 PL011 UART — direct access for early boot (before bug() works) */
#define PL011_BASE  0xFE201000UL
#define PL011_DR    0x00
#define PL011_FR    0x18
#define PL011_FR_TXFF (1 << 5)

/* Provided by intvecs.S */
extern void VectorTable(void);

/* Forward declarations */
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puthex(uint64_t val);
static void clear_bss(struct TagItem *msg);
static void setup_vectors(void);
void kernel_cstart(struct TagItem *msg);

/* Globals — defined in kernel_startup.c */
extern struct AARCH64_Implementation __aarch64_arosintern;
extern struct ExecBase *SysBase;
extern struct TagItem *BootMsg;

/* Stack for the kernel (40KB) -- in .data so clear_bss does not zero it */
static uint64_t stack[5120] __attribute__((used, aligned(16), section(".data")));

/*
 * _start — entry point, called by bootstrap.
 * x0 = pointer to TagItem list from bootstrap.
 * Placed in .text.startup so linker script can order it first.
 */
__asm__ (
    ".section .text.startup, \"ax\"\n"
    ".globl _start\n"
    ".type _start, %function\n"
    "_start:\n"
    "   msr  spsel, #1\n"          /* EL1h: use SP_EL1 everywhere */
    "   adrp x1, stack + 40960\n"
    "   add  x1, x1, :lo12:stack + 40960\n"
    "   mov  sp, x1\n"
    "   b    kernel_cstart\n"
    ".text\n"
);

/*
 * kernel_cstart — main kernel initialization.
 * Follows the same sequence as arch/arm-native/kernel/kernel_startup.c.
 */
void __attribute__((noinline)) kernel_cstart(struct TagItem *msg)
{
    UWORD *ranges[3];
    struct MemHeader *mh;
    unsigned long memlower = 0, memupper = 0;
    unsigned long protlower = 0, protupper = 0;
    struct TagItem *tag;
    tls_t *__tls;

    BootMsg = msg;

    uart_puts("\n[Kernel] AROS AArch64 Kernel (" __DATE__ ")\n");

    /* Clear BSS */
    clear_bss(msg);

    /* Install exception vectors */
    setup_vectors();

    /* Probe CPU */
    {
        uint64_t midr, cntfrq;
        __asm__ volatile("mrs %0, midr_el1" : "=r"(midr));
        __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(cntfrq));
        __aarch64_arosintern.ARMI_Family = 8;

        uart_puts("[Kernel] CPU: ");
        uint32_t part = (midr >> 4) & 0xFFF;
        if (part == 0xD08) uart_puts("Cortex-A72");
        else if (part == 0xD0B) uart_puts("Cortex-A76");
        else { uart_puts("Unknown-"); uart_puthex(part); }
        uart_puts(", Timer: ");
        uart_puthex(cntfrq);
        uart_puts(" Hz\n");
    }

    /* Parse boot tags */
    tag = msg;
    while (tag->ti_Tag != TAG_DONE)
    {
        switch (tag->ti_Tag)
        {
        case KRN_MEMLower:
            memlower = tag->ti_Data;
            break;
        case KRN_MEMUpper:
            memupper = tag->ti_Data;
            break;
        case KRN_ProtAreaStart:
            protlower = tag->ti_Data;
            break;
        case KRN_ProtAreaEnd:
            protupper = (tag->ti_Data + 4095) & ~4095UL;
            break;
        case KRN_Platform:
            __aarch64_arosintern.ARMI_Platform = tag->ti_Data;
            break;
        }
        tag++;
    }

    uart_puts("[Kernel] Memory: ");
    uart_puthex(memlower);
    uart_puts(" - ");
    uart_puthex(memupper);
    uart_puts("\n");

    /* Allocate TLS in protected area */
    __tls = (tls_t *)protupper;
    protupper += (sizeof(tls_t) + 4095) & ~4095UL;

    __tls->SysBase = NULL;
    __tls->KernelBase = NULL;
    __tls->ThisTask = NULL;
    __tls->IDNestCnt = -1;
    __tls->TDNestCnt = -1;
    __tls->SupervisorCount = 0;
    __tls->ScheduleData = NULL;

    /* Set TLS pointer in TPIDR_EL1 */
    __asm__ volatile("msr tpidr_el1, %0" : : "r"(__tls));

    uart_puts("[Kernel] TLS @ ");
    uart_puthex((uint64_t)__tls);
    uart_puts("\n");

    /* Adjust memory lower bound past protected area */
    if (memlower >= protlower)
        memlower = protupper;

    /* --- Memory and ExecBase initialization --- */

    mh = (struct MemHeader *)memlower;

    uart_puts("[Kernel] Creating TLSF memory @ ");
    uart_puthex(memlower);
    uart_puts(", size ");
    uart_puthex(memupper - memlower);
    uart_puts("\n");

    /* Initialize TLSF memory allocator */
    krnCreateTLSFMemHeader("System Memory", 0, mh,
        (memupper - memlower),
        MEMF_FAST | MEMF_PUBLIC | MEMF_KICK | MEMF_LOCAL);

    /* Protect the bootstrap area from allocation */
    if (memlower < protlower)
    {
        ((struct MemHeaderExt *)mh)->mhe_AllocAbs(
            (struct MemHeaderExt *)mh,
            protupper - protlower, (void *)protlower);
    }

    /*
     * Allocate per-CPU scheduler data for boot CPU.
     * Must happen before krnPrepareExecBase() because exec_init sets
     * SCHEDELAPSED_SET(SCHEDQUANTUM_GET) which needs ScheduleData.
     * Use TLSF directly since AllocMem requires SysBase.
     */
    {
        extern void core_InitScheduleData(struct AArch64SchedulerPrivate *);
        ULONG allocFlags = MEMF_PUBLIC | MEMF_CLEAR;
        struct AArch64SchedulerPrivate *schedData = 
            ((struct MemHeaderExt *)mh)->mhe_Alloc(
                (struct MemHeaderExt *)mh,
                sizeof(struct AArch64SchedulerPrivate),
                &allocFlags);
        if (schedData) {
            core_InitScheduleData(schedData);
            __tls->ScheduleData = schedData;
        }
        uart_puts("[Kernel] ScheduleData @ ");
        uart_puthex((uint64_t)schedData);
        uart_puts("\n");
    }

    /* Kernel ROM ranges for resident scanning */
    ranges[0] = (UWORD *)krnGetTagData(KRN_KernelLowest, 0, BootMsg);
    ranges[1] = (UWORD *)krnGetTagData(KRN_KernelHighest, 0, BootMsg);
    ranges[2] = (UWORD *)-1;

    uart_puts("[Kernel] Preparing ExecBase...\n");
    krnPrepareExecBase(ranges, mh, BootMsg);

    __tls->SysBase = SysBase;

    uart_puts("[Kernel] SysBase @ ");
    uart_puthex((uint64_t)SysBase);
    uart_puts("\n");

    D(bug("[Kernel] SysBase @ 0x%p, KernelBase @ 0x%p\n",
          SysBase, __tls->KernelBase));

    /* --- Framebuffer init --- */
    extern int vcfb_init(void);
    extern void fb_Putc(char chr);
    if (vcfb_init())
    {
        /* Print banner on screen */
        const char *banner = "[Kernel] AROS AArch64 on Raspberry Pi 4\n";
        while (*banner) fb_Putc(*banner++);
    }

    /* --- GIC-400 and timer initialization --- */

    /* BCM2711_GICD_BASE / BCM2711_GICC_BASE from kernel_aarch64.h */
    uart_puts("[Kernel] Initializing GIC-400...\n");
    gic400_Init(BCM2711_GICD_BASE, BCM2711_GICC_BASE);

    /* Timer init — must be after GIC, before InitCode */
    extern void timer_Init(unsigned long gicd_base);
    timer_Init(BCM2711_GICD_BASE);

    /* Enable IRQs at CPU level */
    __asm__ volatile("msr daifclr, #2");  /* Clear IRQ mask bit */
    uart_puts("[Kernel] IRQs enabled\n");

    /* --- Run resident modules --- */

    uart_puts("[Kernel] InitCode(RTF_SINGLETASK)...\n");

    /* Ensure IDNestCnt is -1 (interrupts enabled) before InitCode */
    IDNESTCOUNT_SET(-1);
    TDNESTCOUNT_SET(-1);

    /*
     * Switch the bootstrap task to an exec-allocated stack BEFORE
     * InitCode runs any modules. The kernel stack is in the kernel
     * RW area which is not in exec's memory list. SMP exec's
     * ASSERT_VALID_PTR uses TypeOfMem() to validate pointers, and
     * local variables on the kernel stack fail this check.
     */
    {
        struct Task *t = GET_THIS_TASK;
        APTR newStack = AllocMem(AROS_STACKSIZE, MEMF_PUBLIC);
        if (newStack && t)
        {
            t->tc_SPLower = newStack;
            t->tc_SPUpper = newStack + AROS_STACKSIZE;
            t->tc_SPReg = t->tc_SPUpper;
            __asm__ volatile(
                "mov sp, %0"
                : : "r"((IPTR)t->tc_SPUpper - 16)
                : "memory"
            );
        }
    }

    /*
     * Register the kernel memory area with exec so TypeOfMem()
     * recognizes kernel-space pointers. This is needed because
     * interrupt handlers run on SP_EL1 (kernel stack), and SMP
     * exec's ASSERT_VALID_PTR validates stack-local pointers.
     *
     * We create a minimal MemHeader with mh_Free=0 so AllocMem
     * never allocates from it — it's purely for TypeOfMem.
     */
    {
        IPTR klow = krnGetTagData(KRN_KernelLowest, 0, BootMsg);
        IPTR khigh = krnGetTagData(KRN_KernelHighest, 0, BootMsg);
        if (klow && khigh)
        {
            static struct MemHeader kernelMH;
            kernelMH.mh_Node.ln_Type = NT_MEMORY;
            kernelMH.mh_Node.ln_Name = "Kernel Memory";
            kernelMH.mh_Node.ln_Pri = -128;
            kernelMH.mh_Attributes = MEMF_KICK | MEMF_LOCAL;
            kernelMH.mh_Lower = (APTR)klow;
            kernelMH.mh_Upper = (APTR)khigh;
            kernelMH.mh_First = NULL;
            kernelMH.mh_Free = 0;
            Forbid();
            Enqueue(&SysBase->MemList, &kernelMH.mh_Node);
            Permit();
        }
    }

    InitCode(RTF_SINGLETASK, 0);

    /*
     * Patch AllocMem to ignore MEMF_CHIP — AArch64 has no chip memory.
     * Must happen after RTF_SINGLETASK (kernel.resource init) but before
     * RTF_COLDSTART (graphics/intuition init which allocate sprite data
     * with MEMF_CHIP).
     */
    {
        #include <defines/exec_LVO.h>
        extern void *(*__chip_AllocMem)();
        __chip_AllocMem = SetFunction((struct Library *)SysBase,
            -LVOAllocMem * LIB_VECTSIZE,
            AROS_SLIB_ENTRY(AllocMem, Kernel, LVOAllocMem));
    }

    uart_puts("[Kernel] InitCode(RTF_COLDSTART)...\n");

    /* Wake secondary cores before COLDSTART (exec + GIC + timer are ready) */
    extern void smp_wake_cores(void);
    smp_wake_cores();

    InitCode(RTF_COLDSTART, 0);

    /*
     * If we get here, no COLDSTART resident took over.
     * This is expected until we have timer.device, DOS, etc.
     */
    uart_puts("[Kernel] exec.library is alive! SysBase @ ");
    uart_puthex((uint64_t)SysBase);
    uart_puts("\n");

    /* Verify timer is ticking — wait for first 50 ticks (1 second) */
    extern uint64_t timer_GetTickCount(void);
    while (timer_GetTickCount() < 50)
        __asm__ volatile("wfe");
    uart_puts("[Kernel] Timer OK: ");
    uart_puthex(timer_GetTickCount());
    uart_puts(" ticks\n");

    uart_puts("[Kernel] System idle (no COLDSTART residents yet).\n");
    for (;;) __asm__ volatile("wfe");
}

/* --- Helper functions --- */

static void clear_bss(struct TagItem *msg)
{
    struct TagItem *tag = msg;
    while (tag->ti_Tag != TAG_DONE)
    {
        if (tag->ti_Tag == KRN_KernelBss && tag->ti_Data)
        {
            struct KernelBSS *bss = (struct KernelBSS *)tag->ti_Data;
            while (bss->addr && bss->len)
            {
                uint8_t *p = (uint8_t *)bss->addr;
                unsigned long len = bss->len;
                while (len--) *p++ = 0;
                bss++;
            }
            return;
        }
        tag++;
    }
}

static void setup_vectors(void)
{
    __asm__ volatile("msr vbar_el1, %0; isb" : : "r"((uint64_t)&VectorTable));
}

/* Minimal UART output — no dependencies, for early boot before bug() works */
void uart_putc(char c)
{
    while (*(volatile uint32_t *)(PL011_BASE + PL011_FR) & PL011_FR_TXFF) ;
    if (c == '\n') {
        *(volatile uint32_t *)(PL011_BASE + PL011_DR) = '\r';
        while (*(volatile uint32_t *)(PL011_BASE + PL011_FR) & PL011_FR_TXFF) ;
    }
    *(volatile uint32_t *)(PL011_BASE + PL011_DR) = c;
}

void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

void uart_puthex(uint64_t val)
{
    const char h[] = "0123456789abcdef";
    int started = 0;
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        int d = (val >> i) & 0xF;
        if (d || started || i == 0) { uart_putc(h[d]); started = 1; }
    }
}

/*
 * ExceptionHandler — called from intvecs.S for non-SVC synchronous exceptions
 * and SError.
 *
 * For data aborts (EC=0x25) and prefetch aborts (EC=0x21) that occur while a
 * task is running, we call the task's trap handler (Exec_TrapHandler) which
 * redirects the task's PC to Exec_CrashHandler.  The exception then returns
 * to the task (via eret) and the crash handler calls Alert().
 *
 * The frame pointer points to a full ExceptionContext on SP_EL1:
 *   [0..28]  = x0-x28 (r[0]-r[28])
 *   [29]     = x29 (fp)
 *   [30]     = x30 (lr)
 *   [31]     = sp
 *   [32]     = ELR_EL1 (pc)
 *   [33]     = SPSR_EL1 (pstate)
 *
 * Returns 0 to halt, 1 if the exception was handled (eret back to task).
 */
int ExceptionHandler(uint64_t exception, uint64_t *frame)
{
    uint64_t esr, far;
    uint32_t ec;

    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));

    ec = (esr >> 26) & 0x3F;

    /* Print diagnostic */
    uart_puts("\n*** EXCEPTION ");
    uart_puthex(exception);
    uart_puts(" ***\n  ESR_EL1: ");
    uart_puthex(esr);
    uart_puts(" (EC=");
    uart_puthex(ec);
    uart_puts(")\n  ELR_EL1: ");
    uart_puthex(frame[32]);
    uart_puts("\n  FAR_EL1: ");
    uart_puthex(far);
    uart_puts("\n  SP: ");
    uart_puthex(frame[31]);
    uart_puts("\n  LR: ");
    uart_puthex(frame[30]);
    uart_puts("  FP: ");
    uart_puthex(frame[29]);
    uart_puts("\n  x0="); uart_puthex(frame[0]);
    uart_puts(" x1="); uart_puthex(frame[1]);
    uart_puts("\n  x2="); uart_puthex(frame[2]);
    uart_puts(" x3="); uart_puthex(frame[3]);
    uart_puts("\n  x16="); uart_puthex(frame[16]);
    uart_puts(" x17="); uart_puthex(frame[17]);
    uart_puts("\n  x19="); uart_puthex(frame[19]);
    uart_puts(" x20="); uart_puthex(frame[20]);
    uart_puts("\n  x21="); uart_puthex(frame[21]);
    uart_puts(" x22="); uart_puthex(frame[22]);
    uart_puts("\n");
    /* Dump stack */
    {
        uint64_t sp = frame[31];
        int i;
        uart_puts("  Stack:\n");
        for (i = 0; i < 8; i++) {
            uart_puts("    ");
            uart_puthex(sp + i * 16);
            uart_puts(": ");
            uart_puthex(*(uint64_t *)(sp + i * 16));
            uart_puts(" ");
            uart_puthex(*(uint64_t *)(sp + i * 16 + 8));
            uart_puts("\n");
        }
    }

    /*
     * Try to recover from data/prefetch aborts when a task is running.
     * EC=0x20/0x24: Instruction Abort
     * EC=0x21/0x25: Data Abort
     */
    if (SysBase && (ec == 0x25 || ec == 0x24 || ec == 0x21 || ec == 0x20 || ec == 0x00))
    {
        struct Task *t = GET_THIS_TASK;

        if (t)
        {
            ULONG trapCode = (ec == 0x00) ? 4 : (ec == 0x25 || ec == 0x24) ? 2 : 3;

            uart_puts("  Task: ");
            if (t->tc_Node.ln_Name)
                uart_puts(t->tc_Node.ln_Name);
            uart_puts("\n");

            /*
             * The saved frame IS an ExceptionContext (same layout).
             * Cast it directly — no need to copy field by field.
             */
            struct ExceptionContext *ctx = (struct ExceptionContext *)frame;

            /* Call the trap handler */
            {
                void (*trapHandler)(ULONG, void *) = SysBase->TaskTrapCode;

                if (t->tc_TrapCode)
                    trapHandler = t->tc_TrapCode;

                if (trapHandler)
                {
                    trapHandler(trapCode, ctx);

                    /*
                     * The trap handler may have redirected ctx.pc to
                     * Exec_CrashHandler.  Write the modified PC back
                     * into the saved frame so eret returns there.
                     */
                    /* ctx IS the frame, so writes are already in place */

                    uart_puts("  Redirected PC to ");
                    uart_puthex(ctx->pc);
                    uart_puts("\n");

                    return 1;  /* Tell asm stub to eret */
                }
            }
        }
    }

    uart_puts("  *** UNRECOVERABLE — system halted ***\n");
    return 0;  /* Tell asm stub to halt */
}

void InterruptHandler(void)
{
    /*
     * Bump SupervisorCount again so core_ExitInterrupt (called from
     * the ASM IRQStub after we return) sees count > 1 during the
     * actual interrupt processing, and only attempts rescheduling
     * after the handler has fully completed.
     */
    tls_t *__tls;
    __asm__ volatile("mrs %0, tpidr_el1" : "=r"(__tls));
    __tls->SupervisorCount++;

    gic400_HandleIRQ(gic400_GetGICCBase());

    __tls->SupervisorCount--;
}


