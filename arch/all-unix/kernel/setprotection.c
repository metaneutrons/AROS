/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: KrnSetProtection for unix-hosted — wraps mprotect with page alignment.
          On darwin-aarch64, also flushes the icache via sys_icache_invalidate.
*/

#include <aros/kernel.h>
#include <aros/libcall.h>

#include <inttypes.h>

#include <kernel_base.h>
#include <kernel_intern.h>

#include <sys/mman.h>

#define PAGE_SIZE 16384
#define PAGE_MASK (~(IPTR)(PAGE_SIZE - 1))

AROS_LH3I(void, KrnSetProtection,
         AROS_LHA(void *, address, A0),
         AROS_LHA(uint32_t, length, D0),
         AROS_LHA(KRN_MapAttr, flags, D1),
         struct KernelBase *, KernelBase, 21, Kernel)
{
    AROS_LIBFUNC_INIT

    int prot = 0;

    if (flags & MAP_Readable)
        prot |= PROT_READ;
    if (flags & MAP_Writable)
        prot |= PROT_WRITE;
    if (flags & MAP_Executable)
        prot |= PROT_EXEC;

    /* Page-align for mprotect */
    IPTR start = (IPTR)address & PAGE_MASK;
    IPTR end   = ((IPTR)address + length + PAGE_SIZE - 1) & PAGE_MASK;

#if defined(HOST_OS_darwin) && defined(__aarch64__)
    /* Flush icache before making pages executable */
    if (flags & MAP_Executable)
    {
        KernelBase->kb_PlatformData->iface->icache_invalidate(address, length);
        AROS_HOST_BARRIER
    }
#endif

    KernelBase->kb_PlatformData->iface->mprotect((void *)start, end - start, prot);
    AROS_HOST_BARRIER

    AROS_LIBFUNC_EXIT
}
