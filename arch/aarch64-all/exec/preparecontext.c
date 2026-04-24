/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: PrepareContext() - Prepare a task context for dispatch, AArch64 version.

    Initializes the task's ExceptionContext (et_RegFrame) so that when
    cpu_Dispatch() copies it to the kernel stack frame and the ASM does
    eret, the task starts executing at entryPoint with the given arguments.
*/

#include <exec/execbase.h>
#include <exec/memory.h>
#include <utility/tagitem.h>
#include <proto/arossupport.h>
#include <proto/kernel.h>
#include <aros/aarch64/cpucontext.h>

#include "exec_intern.h"
#include "exec_util.h"
#if defined(__AROSEXEC_SMP__)
#include "etask.h"
#endif

BOOL PrepareContext(struct Task *task, APTR entryPoint, APTR fallBack,
                    const struct TagItem *tagList, struct ExecBase *SysBase)
{
    struct TagItem *t;
    struct ExceptionContext *ctx;
    IPTR args[8] = {0};
    int numargs = 0;

    if (!(task->tc_Flags & TF_ETASK))
        return FALSE;

    ctx = KrnCreateContext();
    task->tc_UnionETask.tc_ETask->et_RegFrame = ctx;
    if (!ctx)
        return FALSE;

    /* Collect arguments from tags */
    while ((t = LibNextTagItem((struct TagItem **)&tagList)))
    {
        switch (t->ti_Tag)
        {
#if defined(__AROSEXEC_SMP__)
            case TASKTAG_AFFINITY:
                IntETask(task->tc_UnionETask.tc_ETask)->iet_CpuAffinity = (cpumask_t *)t->ti_Data;
                break;
#endif
#define REGARG(x)                           \
            case TASKTAG_ARG ## x:          \
                args[x - 1] = t->ti_Data;  \
                if (x > numargs)            \
                    numargs = x;            \
                break;
            REGARG(1) REGARG(2) REGARG(3) REGARG(4)
            REGARG(5) REGARG(6) REGARG(7) REGARG(8)
#undef REGARG
        }
    }

    /*
     * Initialize ExceptionContext fields.
     * cpu_Dispatch() will copy this to the kernel stack frame,
     * and the ASM restore sequence will load all registers and eret.
     */

    /* Arguments in x0-x7 */
    {
        int i;
        for (i = 0; i < numargs && i < 8; i++)
            ctx->r[i] = args[i];
    }

    /* Frame pointer = 0 (end of call chain) */
    ctx->fp = 0;

    /* Link register = fallBack (where task returns to) */
    ctx->lr = (IPTR)fallBack;

    /* Stack pointer = task's stack */
    ctx->sp = (IPTR)task->tc_SPReg;

    /* Entry point */
    ctx->pc = (IPTR)entryPoint;

    /* SPSR: EL1h (SPSel=1), DAIF clear (interrupts enabled) */
    ctx->pstate = 0x00000005;

    ctx->Flags = 0;

    return TRUE;
}
