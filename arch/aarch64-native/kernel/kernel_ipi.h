/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IPI (Inter-Processor Interrupt) definitions for AArch64 SMP.
          Minimal implementation — IPI via GIC SGI to be added later.
*/

#ifndef __KERNEL_IPI_H_
#define __KERNEL_IPI_H_

#include <aros/types/spinlock_s.h>
#include <utility/hooks.h>
#include "kernel_base.h"

/* IPI message types */
#define IPI_NOP         0
#define IPI_STOP        1
#define IPI_RESUME      2
#define IPI_RESCHEDULE  3
#define IPI_CALL_HOOK   4
#define IPI_CAUSE       5

#define IPI_CALL_HOOK_MAX_ARGS  5

/* IPI hook for cross-CPU function calls */
struct IPIHook
{
    struct Hook     ih_Hook;
    IPTR            ih_Args[IPI_CALL_HOOK_MAX_ARGS];
    uint32_t *      ih_CPUDone;
    uint32_t *      ih_CPURequested;
    int             ih_Async;
    spinlock_t      ih_Lock;
    spinlock_t      ih_SyncLock;
};

/* Stub implementations — real IPI via GIC SGI to be added */
static inline void core_DoIPI(uint8_t ipi_number, void *cpu_mask, struct KernelBase *KernelBase)
{
    /* TODO: Send GIC SGI to target CPUs */
    (void)ipi_number;
    (void)cpu_mask;
    (void)KernelBase;
}

static inline int core_DoCallIPI(struct Hook *hook, void *cpu_mask, int async, int nargs, IPTR *args, APTR _KB)
{
    /* TODO: Implement cross-CPU hook calls */
    (void)hook;
    (void)cpu_mask;
    (void)async;
    (void)nargs;
    (void)args;
    (void)_KB;
    return 0;
}

#endif /* __KERNEL_IPI_H_ */
