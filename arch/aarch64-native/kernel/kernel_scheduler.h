#ifndef KERNEL_SCHEDULER_H
#define KERNEL_SCHEDULER_H
/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Per-CPU scheduler data for AArch64 SMP.
          Modeled after arch/all-pc/kernel/kernel_scheduler.h.
*/

#include <aros/config.h>

#if defined(__AROSEXEC_SMP__)
#include <exec/tasks.h>

struct AArch64SchedulerPrivate
{
    struct Task         *RunningTask;   /* Currently running task on this core */
    ULONG               ScheduleFlags;
    UWORD               Granularity;    /* Length of one heartbeat tick */
    UWORD               Quantum;        /* # of heartbeat ticks a task may run */
    UWORD               Elapsed;        /* # of heartbeat ticks the current task has run */
    BYTE                IDNestCnt;
    BYTE                TDNestCnt;
};

void core_InitScheduleData(struct AArch64SchedulerPrivate *schedData);
#endif

BOOL core_Schedule(void);
void core_Switch(void);
struct Task *core_Dispatch(void);

#endif /* KERNEL_SCHEDULER_H */
