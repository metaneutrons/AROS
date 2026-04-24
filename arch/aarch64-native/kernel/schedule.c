/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: KrnSchedule — run task scheduling, AArch64.
*/

#include <aros/kernel.h>
#include <kernel_base.h>
#include <kernel_syscall.h>
#include <proto/kernel.h>

#define AROS_NO_ATOMIC_OPERATIONS
#include "exec_platform.h"
#include "tls.h"

AROS_LH0(void, KrnSchedule,
    struct KernelBase *, KernelBase, 6, Kernel)
{
    AROS_LIBFUNC_INIT

    /*
     * If called from interrupt/supervisor context (e.g. from Signal()
     * inside a soft interrupt handler), just set the reschedule flag.
     * The actual switch will happen when core_ExitInterrupt checks it.
     * Triggering an SVC from interrupt context causes deep nesting.
     */
    tls_t *__tls;
    __asm__ volatile("mrs %0, tpidr_el1" : "=r"(__tls));
    if (__tls->SupervisorCount > 0)
    {
        FLAG_SCHEDSWITCH_SET;
        return;
    }

    krnSysCall(SC_SCHEDULE);

    AROS_LIBFUNC_EXIT
}
