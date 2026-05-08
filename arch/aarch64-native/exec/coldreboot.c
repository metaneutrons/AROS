/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
     Author: Fabian Schmieder

    Desc: ColdReboot() - Reboot via BCM2711 Power Management Watchdog.
*/

#include <exec/types.h>
#include <exec/execbase.h>
#include <aros/libcall.h>
#include <aros/macros.h>
#include <proto/exec.h>
#include <proto/kernel.h>

#include "exec_intern.h"
#include "exec_util.h"

/*
 * BCM2711 Power Management registers.
 * The watchdog timer triggers a full SoC reset when it expires.
 */
#define PM_OFFSET       0x100000
#define PM_RSTC         0x1C
#define PM_WDOG         0x24

#define PM_PASSWORD     0x5A000000
#define PM_RSTC_WRCFG_FULL_RESET 0x00000020

AROS_LH0(void, ColdReboot,
    struct ExecBase *, SysBase, 121, Exec)
{
    AROS_LIBFUNC_INIT

    IPTR peribase;

    Exec_DoResetCallbacks((struct IntExecBase *)SysBase, SD_ACTION_WARMREBOOT);

    /* Get peripheral base from kernel */
    peribase = KrnGetSystemAttr(KATTR_PeripheralBase);
    if (peribase == 0)
        peribase = 0xFE000000; /* BCM2711 default */

    /* Disable interrupts */
    asm volatile("msr daifset, #0xF" ::: "memory");

    /* Set watchdog timeout to minimum (1 tick ≈ 16µs) */
    *(volatile ULONG *)(peribase + PM_OFFSET + PM_WDOG) =
        AROS_LONG2LE(PM_PASSWORD | 1);

    /* Trigger full reset via RSTC */
    *(volatile ULONG *)(peribase + PM_OFFSET + PM_RSTC) =
        AROS_LONG2LE(PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET);

    /* Wait for reset */
    for (;;)
        asm volatile("wfi");

    AROS_LIBFUNC_EXIT
}
