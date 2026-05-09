/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Misc syscall handler for AArch64.
*/

#include <exec/execbase.h>
#include <proto/exec.h>

/*
 * HandleSyscall — called from ASM for non-switch SVCs.
 * Currently unused — all syscalls go through core_SysCall().
 */
void HandleSyscall(unsigned long syscall_num)
{
    (void)syscall_num;
}
