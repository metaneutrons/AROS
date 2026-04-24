/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: KrnSpinUnLock for AArch64 — release spinlock with store-release.
*/

#include <aros/types/spinlock_s.h>
#include <aros/kernel.h>
#include <aros/libcall.h>
#include <kernel_base.h>

AROS_LH1(void, KrnSpinUnLock,
    AROS_LHA(spinlock_t *, lock, A1),
    struct KernelBase *, KernelBase, 53, Kernel)
{
    AROS_LIBFUNC_INIT

    /* Single-CPU: decrement nesting, unmask IRQs when outermost lock released */
    if (--lock->lock == 0)
        __asm__ volatile("msr daifclr, #3" ::: "memory");

    AROS_LIBFUNC_EXIT
}
