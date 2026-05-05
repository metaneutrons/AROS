#ifndef RP1_H
#define RP1_H

#include <exec/types.h>
#include <exec/libraries.h>

/* RP1 peripheral offsets within BAR1 (from kernel_aarch64.h) */
#include "kernel_aarch64.h"

struct RP1Base {
    struct Library  rp1_Lib;
    BOOL            rp1_Present;
    IPTR            rp1_BAR1;       /* PCIe BAR1 base address */

    /* Pre-computed peripheral base addresses */
    IPTR            rp1_USB0;
    IPTR            rp1_USB1;
    IPTR            rp1_ETH;
    IPTR            rp1_GPIO;
    IPTR            rp1_I2C0;
    IPTR            rp1_I2C1;
    IPTR            rp1_UART0;
};

/* Global instance (set during init) */
extern struct RP1Base *RP1;

/*
 * Helper for drivers: get peripheral base.
 * Returns RP1 BAR1 + offset on RPi5, or peribase + offset on RPi4.
 */
static inline IPTR rp1_get_peripheral(IPTR rp1_offset, IPTR bcm2711_offset)
{
    if (RP1 && RP1->rp1_Present)
        return RP1->rp1_BAR1 + rp1_offset;
    else
        return BCM2711_PERIBASE + bcm2711_offset;
}

#endif /* RP1_H */
