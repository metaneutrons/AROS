/*
    Copyright (C) 2011-2026, The AROS Development Team. All rights reserved.

    Desc: Create a seglist for ROM code.
*/

#define AROS_LIBREQ(base,ver)   /* We test for versions manually */

#include <aros/debug.h>
#include <proto/exec.h>

struct phony_segment
{
    ULONG Size;
    BPTR  Next;
} __attribute__((packed));

#include <proto/arossupport.h>

        BPTR __CreateSegList(
        APTR function, struct ExecBase *SysBase )
{
    struct phony_segment *segtmp;
    struct FullJumpVec *Code;

    segtmp = AllocMem(sizeof(*segtmp) + sizeof(*Code), MEMF_ANY);
    if (!segtmp)
        return BNULL;

    Code = (struct FullJumpVec *)((IPTR)segtmp + sizeof(*segtmp));
    segtmp->Size = sizeof(*segtmp) + sizeof(*Code);
    segtmp->Next = (BPTR)0;
    __AROS_SET_FULLJMP(Code, function);

    if (SysBase->LibNode.lib_Version >= 36)
        CacheClearE(Code, sizeof(*Code), CACRF_ClearI | CACRF_ClearD);

    D(bug("[CreateSegList] seg=%p code=%p target=%p\n", MKBADDR(&segtmp->Next), Code, function));

    return MKBADDR(&segtmp->Next);
}
