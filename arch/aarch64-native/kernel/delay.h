/*
 * Calibrated microsecond delay using ARM Generic Timer (CNTPCT_EL0).
 *
 * The Generic Timer runs at a fixed frequency (typically 54MHz on BCM2711/2712).
 * CNTFRQ_EL0 gives the frequency, CNTPCT_EL0 gives the current count.
 * This provides accurate delays independent of CPU clock speed.
 */

#ifndef AROS_AARCH64_DELAY_H
#define AROS_AARCH64_DELAY_H

#include <exec/types.h>

static inline ULONG timer_get_freq(void)
{
    ULONG freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

static inline UQUAD timer_get_count(void)
{
    UQUAD count;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(count));
    return count;
}

/*
 * Delay for the specified number of microseconds.
 * Uses the ARM Generic Timer — works on both RPi4 and RPi5.
 */
static inline void udelay_calibrated(ULONG us)
{
    ULONG freq = timer_get_freq();
    UQUAD target = timer_get_count() + ((UQUAD)freq * us) / 1000000;

    while (timer_get_count() < target)
        __asm__ volatile("yield");
}

/*
 * Delay for the specified number of milliseconds.
 */
static inline void mdelay_calibrated(ULONG ms)
{
    udelay_calibrated(ms * 1000);
}

#endif /* AROS_AARCH64_DELAY_H */
