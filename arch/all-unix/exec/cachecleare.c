/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: CacheClearE() for unix-hosted — minimal implementation.
*/

#include <exec/types.h>
#include <exec/execbase.h>
#include <aros/libcall.h>
#include <proto/exec.h>

AROS_LH3(void, CacheClearE,
        AROS_LHA(APTR, address, A0),
        AROS_LHA(IPTR, length, D0),
        AROS_LHA(ULONG, caches, D1),
        struct ExecBase *, SysBase, 107, Exec)
{
    AROS_LIBFUNC_INIT
    /* no-op for now */
    AROS_LIBFUNC_EXIT
}
