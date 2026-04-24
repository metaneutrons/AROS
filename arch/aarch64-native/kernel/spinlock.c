/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: KrnSpinLock for AArch64 — single-CPU: interrupt masking with nesting.
*/

#include <aros/types/spinlock_s.h>
#include <aros/kernel.h>
#include <aros/libcall.h>
#include <utility/hooks.h>
#include <kernel_base.h>

AROS_LH3(spinlock_t *, KrnSpinLock,
    AROS_LHA(spinlock_t *, lock, A1),
    AROS_LHA(struct Hook *, failhook, A0),
    AROS_LHA(ULONG, mode, D0),
    struct KernelBase *, KernelBase, 52, Kernel)
{
    AROS_LIBFUNC_INIT

    /*
     * Single-CPU SMP: use the lock field as a nesting counter with
     * interrupt masking. Mask IRQs on first acquire, track nesting
     * so unlock only unmasks when the outermost lock is released.
     * When true multi-CPU SMP scheduling is active, replace with
     * a proper ldaxr/stxr spinlock.
     */
    if (lock->lock++ == 0)
        __asm__ volatile("msr daifset, #3" ::: "memory");

    return lock;

    AROS_LIBFUNC_EXIT
}
