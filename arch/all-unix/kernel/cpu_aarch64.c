/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: UNIX signal-to-exception mapping for AArch64 hosted
*/

#include "kernel_base.h"
#include "kernel_intern.h"

#include <signal.h>

/*
 * AArch64 exceptions mapped from UNIX signals:
 *  0 - Reset (not used)
 *  1 - Data abort       (SIGBUS, SIGSEGV)
 *  2 - FIQ              (not simulated)
 *  3 - IRQ              (not simulated)
 *  4 - Prefetch abort   (SIGBUS)
 *  5 - Invalid instruction (SIGILL, SIGTRAP)
 * 11 - FPE              (SIGFPE)
 */

struct SignalTranslation const sigs[] = {
    {SIGILL   ,  4,  5},
    {SIGTRAP  ,  9,  5},
    {SIGBUS   ,  2,  1},
    {SIGFPE   , 11,  1},
    {SIGSEGV  ,  2,  1},
    {-1       , -1, -1}
};
