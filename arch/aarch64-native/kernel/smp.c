/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: SMP secondary core initialization for AArch64 (spin-table method).
*/

#include <exec/execbase.h>
#include <exec/memory.h>
#include <proto/exec.h>

#include "kernel_intern.h"
#include "tls.h"
#include "kernel_scheduler.h"

/* From smp_entry.S */
extern void smp_entry(void);
extern uint64_t smp_boot_stack;
extern uint64_t smp_boot_tls;
extern uint64_t smp_boot_vbar;

/* From kernel_cstart.c */
extern void uart_puts(const char *s);
extern void uart_puthex(uint64_t val);

/* Per-core state */
static volatile int smp_core_alive[4];

#define SMP_STACK_SIZE  8192    /* 8KB per secondary core */

/*
 * smp_secondary_init — called by smp_entry.S on each secondary core.
 * Runs in EL1 with per-core stack and TLS set up.
 */
void smp_secondary_init(void)
{
    uint64_t mpidr;
    int core_id;

    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    core_id = mpidr & 0xFF;     /* Aff0 = core number on Pi 4 */

    uart_puts("[SMP] Core ");
    uart_puthex(core_id);
    uart_puts(" alive\n");

    smp_core_alive[core_id] = 1;

    /* Secondary cores idle for now — no task scheduling.
     * Future: register with exec SMP, enter per-core idle loop. */
    for (;;)
    {
        __asm__ volatile("wfe");
    }
}

/*
 * smp_wake_cores — called from platform init after exec is alive.
 * Wakes secondary cores 1-3 via spin-table.
 */
void smp_wake_cores(void)
{
    /* cpu-release-addr from DTB (hardcoded for Pi 4) */
    static const uint64_t release_addr[4] = { 0xd8, 0xe0, 0xe8, 0xf0 };
    extern void VectorTable(void);
    int i;

    uart_puts("[SMP] Waking secondary cores...\n");

    for (i = 1; i < 4; i++)
    {
        /* Allocate per-core stack */
        void *stack = AllocMem(SMP_STACK_SIZE, MEMF_CLEAR | MEMF_PUBLIC);
        if (!stack)
        {
            uart_puts("[SMP] Failed to allocate stack for core ");
            uart_puthex(i);
            uart_puts("\n");
            continue;
        }

        /* Allocate per-core TLS */
        tls_t *tls = AllocMem(sizeof(tls_t), MEMF_CLEAR | MEMF_PUBLIC);
        if (!tls)
        {
            uart_puts("[SMP] Failed to allocate TLS for core ");
            uart_puthex(i);
            uart_puts("\n");
            continue;
        }

        /* Initialize TLS */
        tls->SysBase = SysBase;
        tls->KernelBase = NULL;
        tls->ThisTask = NULL;
        tls->IDNestCnt = -1;
        tls->TDNestCnt = -1;
        tls->SupervisorCount = 0;
        tls->ScheduleData = NULL;

#if defined(__AROSEXEC_SMP__)
        /* Allocate per-CPU scheduler data */
        {
            struct AArch64SchedulerPrivate *schedData =
                AllocMem(sizeof(struct AArch64SchedulerPrivate), MEMF_PUBLIC | MEMF_CLEAR);
            if (schedData) {
                extern void core_InitScheduleData(struct AArch64SchedulerPrivate *);
                core_InitScheduleData(schedData);
                tls->ScheduleData = schedData;
            }
        }
#endif

        /* Set boot parameters (read by smp_entry.S) */
        smp_boot_stack = (uint64_t)stack + SMP_STACK_SIZE;  /* top of stack */
        smp_boot_tls   = (uint64_t)tls;
        smp_boot_vbar  = (uint64_t)VectorTable;

        /* Ensure writes are visible before waking the core */
        __asm__ volatile("dsb sy" ::: "memory");

        /* Write entry point to cpu-release-addr */
        volatile uint64_t *release = (volatile uint64_t *)release_addr[i];
        *release = (uint64_t)smp_entry;

        /* Ensure the write is visible and wake the core */
        __asm__ volatile("dsb sy; sev" ::: "memory");

        /* Wait for core to come alive (with timeout) */
        int timeout = 1000000;
        while (!smp_core_alive[i] && --timeout > 0)
            __asm__ volatile("yield");

        if (smp_core_alive[i])
        {
            uart_puts("[SMP] Core ");
            uart_puthex(i);
            uart_puts(" started\n");
        }
        else
        {
            uart_puts("[SMP] Core ");
            uart_puthex(i);
            uart_puts(" TIMEOUT\n");
        }
    }

    uart_puts("[SMP] All cores initialized\n");
}
