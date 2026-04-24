/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: exec_platform.h for AArch64 native.
          TLS accessed via TPIDR_EL1 system register.
          SMP: per-CPU scheduler data via ScheduleData in TLS.
*/
#ifndef __EXEC_PLATFORM_H
#define __EXEC_PLATFORM_H

#include <aros/config.h>

#include "tls.h"

#define SCHEDQUANTUM_VALUE      4
#define SCHEDGRAN_VALUE         1

#if defined(__AROSEXEC_SMP__)
#include <aros/types/spinlock_s.h>
#include <utility/hooks.h>

extern void Kernel_40_KrnSpinInit(spinlock_t *, void *);
#define EXEC_SPINLOCK_INIT(a) Kernel_40_KrnSpinInit((a), NULL)
extern spinlock_t *Kernel_43_KrnSpinLock(spinlock_t *, struct Hook *, ULONG, void *);
#define EXEC_SPINLOCK_LOCK(a,b) Kernel_43_KrnSpinLock((a), NULL, (b), NULL)
extern void Kernel_44_KrnSpinUnLock(spinlock_t *, void *);
#define EXEC_SPINLOCK_UNLOCK(a) Kernel_44_KrnSpinUnLock((a), NULL)
#endif

#define EXEC_REMTASK_NEEDSSWITCH

#define krnSysCallSwitch() \
    __asm__ volatile("svc #2" ::: "x30", "memory")
#define krnSysCallReschedTask(task, state) \
    do { \
        (task)->tc_State = (state); \
        /* Only Remove if the node is actually linked in a list. \
         * A node is linked if both ln_Succ and ln_Pred are non-NULL \
         * and ln_Pred->ln_Succ == node (i.e., the predecessor points \
         * forward to us). Nodes removed by core_Dispatch have NULL ptrs. */ \
        if ((task)->tc_Node.ln_Succ && (task)->tc_Node.ln_Pred && \
            (task)->tc_Node.ln_Pred->ln_Succ == &(task)->tc_Node) \
            Remove(&(task)->tc_Node); \
        if ((state) != TS_REMOVED) \
            Enqueue(&SysBase->TaskReady, &(task)->tc_Node); \
    } while(0)

struct Exec_PlatformData
{
    /* No platform-specific data by default */
};

/* TLS access via TPIDR_EL1 */
#define __GET_TLS() \
    ({ tls_t *__tls; __asm__ volatile("mrs %0, tpidr_el1" : "=r"(__tls)); __tls; })

/*
 * Nesting count macros.
 * Non-SMP: stored in TLS directly (fast path).
 * SMP: stored in per-CPU ScheduleData.
 */
#if defined(__AROSEXEC_SMP__)

#include "etask.h"
#include "kernel_scheduler.h"

#define IDNESTCOUNT_INC \
    do { struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; if (__s) { __s->IDNestCnt++;} } while(0)
#define IDNESTCOUNT_DEC \
    do { struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; if (__s) __s->IDNestCnt--; } while(0)
#define TDNESTCOUNT_INC \
    do { struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; if (__s) { __s->TDNestCnt++;} } while(0)
#define TDNESTCOUNT_DEC \
    do { struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; if (__s) __s->TDNestCnt--; } while(0)

#define IDNESTCOUNT_GET \
    ({ struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; __s ? __s->IDNestCnt : -1; })
#define IDNESTCOUNT_SET(val) \
    do { struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; if (__s) __s->IDNestCnt = (val); } while(0)
#define TDNESTCOUNT_GET \
    ({ struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; __s ? __s->TDNestCnt : -1; })
#define TDNESTCOUNT_SET(val) \
    do { struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; if (__s) __s->TDNestCnt = (val); } while(0)

#define SCHEDQUANTUM_SET(val) \
    do { struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; if (__s) __s->Quantum = (val); } while(0)
#define SCHEDQUANTUM_GET \
    ({ struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; __s ? __s->Quantum : 0; })
#define SCHEDELAPSED_SET(val) \
    do { struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; if (__s) __s->Elapsed = (val); } while(0)
#define SCHEDELAPSED_GET \
    (*({ struct AArch64SchedulerPrivate *__s = __GET_TLS()->ScheduleData; &__s->Elapsed; }))

#else /* !__AROSEXEC_SMP__ */

#define IDNESTCOUNT_INC     do { __GET_TLS()->IDNestCnt++; } while(0)
#define IDNESTCOUNT_DEC     do { __GET_TLS()->IDNestCnt--; } while(0)
#define TDNESTCOUNT_INC     do { __GET_TLS()->TDNestCnt++; } while(0)
#define TDNESTCOUNT_DEC     do { __GET_TLS()->TDNestCnt--; } while(0)

#define IDNESTCOUNT_GET     ({ __GET_TLS()->IDNestCnt; })
#define IDNESTCOUNT_SET(val) do { __GET_TLS()->IDNestCnt = (val); } while(0)
#define TDNESTCOUNT_GET     ({ __GET_TLS()->TDNestCnt; })
#define TDNESTCOUNT_SET(val) do { __GET_TLS()->TDNestCnt = (val); } while(0)

#define SCHEDQUANTUM_SET(val)   (SysBase->Quantum=(val))
#define SCHEDQUANTUM_GET        (SysBase->Quantum)
#define SCHEDELAPSED_SET(val)   (SysBase->Elapsed=(val))
#define SCHEDELAPSED_GET        (SysBase->Elapsed)

#endif /* __AROSEXEC_SMP__ */

/*
 * Schedule flag operations.
 */
#if defined(AROS_NO_ATOMIC_OPERATIONS)

#define FLAG_SCHEDQUANTUM_CLEAR  do { __GET_TLS()->ScheduleFlags &= ~TLSSF_Quantum; } while(0)
#define FLAG_SCHEDQUANTUM_SET    do { __GET_TLS()->ScheduleFlags |= TLSSF_Quantum; } while(0)
#define FLAG_SCHEDSWITCH_CLEAR   do { __GET_TLS()->ScheduleFlags &= ~TLSSF_Switch; } while(0)
#define FLAG_SCHEDSWITCH_SET     do { __GET_TLS()->ScheduleFlags |= TLSSF_Switch; } while(0)
#define FLAG_SCHEDDISPATCH_CLEAR do { __GET_TLS()->ScheduleFlags &= ~TLSSF_Dispatch; } while(0)
#define FLAG_SCHEDDISPATCH_SET   do { __GET_TLS()->ScheduleFlags |= TLSSF_Dispatch; } while(0)

#else

#include <aros/atomic.h>

#define FLAG_SCHEDQUANTUM_CLEAR  do { AROS_ATOMIC_AND(__GET_TLS()->ScheduleFlags, ~TLSSF_Quantum); } while(0)
#define FLAG_SCHEDQUANTUM_SET    do { AROS_ATOMIC_OR(__GET_TLS()->ScheduleFlags, TLSSF_Quantum); } while(0)
#define FLAG_SCHEDSWITCH_CLEAR   do { AROS_ATOMIC_AND(__GET_TLS()->ScheduleFlags, ~TLSSF_Switch); } while(0)
#define FLAG_SCHEDSWITCH_SET     do { AROS_ATOMIC_OR(__GET_TLS()->ScheduleFlags, TLSSF_Switch); } while(0)
#define FLAG_SCHEDDISPATCH_CLEAR do { AROS_ATOMIC_AND(__GET_TLS()->ScheduleFlags, ~TLSSF_Dispatch); } while(0)
#define FLAG_SCHEDDISPATCH_SET   do { AROS_ATOMIC_OR(__GET_TLS()->ScheduleFlags, TLSSF_Dispatch); } while(0)

#endif

#define FLAG_SCHEDQUANTUM_ISSET  ({ (__GET_TLS()->ScheduleFlags & TLSSF_Quantum) ? TRUE : FALSE; })
#define FLAG_SCHEDSWITCH_ISSET   ({ (__GET_TLS()->ScheduleFlags & TLSSF_Switch) ? TRUE : FALSE; })
#define FLAG_SCHEDDISPATCH_ISSET ({ (__GET_TLS()->ScheduleFlags & TLSSF_Dispatch) ? TRUE : FALSE; })

#define GET_THIS_TASK           TLS_GET(ThisTask)
#if defined(__AROSEXEC_SMP__)
#define SET_THIS_TASK(x)        TLS_SET(ThisTask,(x))

/* Spinlock wrappers — call kernel.resource functions directly */
#include <aros/types/spinlock_s.h>
#ifndef __KERNEL_NO_SPINLOCK_PROTOS__
extern void Kernel_49_KrnSpinInit(spinlock_t *, void *);
extern spinlock_t *Kernel_52_KrnSpinLock(spinlock_t *, struct Hook *, ULONG, void *);
extern void Kernel_53_KrnSpinUnLock(spinlock_t *, void *);
#endif
#define EXEC_SPINLOCK_INIT(a)       Kernel_49_KrnSpinInit((a), NULL)
#define EXEC_SPINLOCK_LOCK(a,b,c)   Kernel_52_KrnSpinLock((a), (b), (c), NULL)
#define EXEC_SPINLOCK_UNLOCK(a)     Kernel_53_KrnSpinUnLock((a), NULL)
#else
#define SET_THIS_TASK(x)        do { TLS_SET(ThisTask,(x)); SysBase->ThisTask = (x); } while(0)
#endif

#endif /* __EXEC_PLATFORM_H */
