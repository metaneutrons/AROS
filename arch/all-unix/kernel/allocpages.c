/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: KrnAllocPages for unix-hosted — uses mmap to allocate page-aligned memory.
*/

#include <aros/libcall.h>
#include <inttypes.h>

#include <kernel_base.h>
#include <kernel_intern.h>

#include <sys/mman.h>

AROS_LH3I(void *, KrnAllocPages,
         AROS_LHA(void *, addr, A0),
         AROS_LHA(uintptr_t, length, D0),
         AROS_LHA(uint32_t, flags, D1),
         struct KernelBase *, KernelBase, 27, Kernel)
{
    AROS_LIBFUNC_INIT

    int prot = 0;
    void *map;

    if (flags & MAP_Readable)
        prot |= PROT_READ;
    if (flags & MAP_Writable)
        prot |= PROT_WRITE;
    if (flags & MAP_Executable)
        prot |= PROT_EXEC;

    map = KernelBase->kb_PlatformData->iface->mmap(NULL, length, prot, MAP_PRIVATE | MAP_ANON, -1, 0);
    AROS_HOST_BARRIER

    if (map == MAP_FAILED)
        return NULL;

    return map;

    AROS_LIBFUNC_EXIT
}
