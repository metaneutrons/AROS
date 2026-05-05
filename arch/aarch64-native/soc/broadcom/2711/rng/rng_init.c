/*
 * BCM2711 RNG200 Hardware Random Number Generator
 *
 * Provides a single function RNGRead() that returns a 32-bit
 * hardware-generated random number from the RNG200 FIFO.
 */

#include <aros/debug.h>
#include <aros/symbolsets.h>
#include <aros/macros.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/kernel.h>

#include "rng_private.h"

#include LC_LIBDEFS_FILE

#define PERIBASE 0xFE000000
#define RNG_OFFSET 0x104000

static int RNG_Init(LIBBASETYPEPTR LIBBASE)
{
    IPTR base = PERIBASE + RNG_OFFSET;

    D(bug("[RNG] Init: base=0x%p\n", base));

    LIBBASE->rng_RegBase = base;

    /* Enable the RNG if not already running */
    if (!(AROS_LE2LONG(*(volatile ULONG *)base) & RNG200_CTRL_ENABLE)) {
        *(volatile ULONG *)base = AROS_LONG2LE(RNG200_CTRL_ENABLE);
        D(bug("[RNG] Enabled RNG200\n"));
    }

    return TRUE;
}

/*
 * RNGRead — return a 32-bit hardware random number.
 *
 * Spins until the FIFO has at least one word available.
 * The FIFO refills automatically from the entropy source.
 */
AROS_LH0(ULONG, RNGRead,
    LIBBASETYPEPTR, LIBBASE, 1, Rng)
{
    AROS_LIBFUNC_INIT

    IPTR base = LIBBASE->rng_RegBase;
    ULONG count;

    /* Wait for FIFO to have data */
    do {
        count = AROS_LE2LONG(*(volatile ULONG *)(base + RNG200_FIFO_COUNT));
    } while ((count >> 8) == 0);

    return AROS_LE2LONG(*(volatile ULONG *)(base + RNG200_FIFO_DATA));

    AROS_LIBFUNC_EXIT
}

ADD2INITLIB(RNG_Init, 0)
