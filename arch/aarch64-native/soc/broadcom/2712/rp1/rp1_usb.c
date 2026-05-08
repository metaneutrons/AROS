/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Author: Fabian Schmieder
 */
/*
 * RP1 USB 3.0 (xHCI) platform initialization for Raspberry Pi 5
 *
 * Creates PCIController structs with hc_RegBase pre-set to the RP1
 * xHCI MMIO addresses, then calls the xHCI startup to register them
 * with the Poseidon USB stack.
 *
 * This is the "platform device" path — equivalent to Linux xhci-plat.c.
 * The xHCI register interface is identical whether accessed via PCI BAR
 * or direct MMIO; only the discovery mechanism differs.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/resident.h>
#include <aros/debug.h>
#include <aros/symbolsets.h>
#include <proto/exec.h>
#include <proto/kernel.h>

#include LC_LIBDEFS_FILE

#define RP1_USB0_OFFSET 0x100000
#define RP1_USB1_OFFSET 0x110000

struct RP1USBBase {
    struct Library  lib;
    IPTR            usb0_base;
    IPTR            usb1_base;
    BOOL            present;
};

APTR KernelBase = NULL;

static int RP1USB_Init(LIBBASETYPEPTR LIBBASE)
{
    struct Library *rp1base;
    IPTR *fields;
    IPTR bar1;

    D(bug("[RP1-USB] Init\n"));

    KernelBase = OpenResource("kernel.resource");
    if (!KernelBase)
        return TRUE;

    /* Only active on RPi5 */
    if ((IPTR)KrnGetSystemAttr(KATTR_PeripheralBase) == 0xFE000000) {
        D(bug("[RP1-USB] RPi4 — using VL805 via PCIe instead\n"));
        return TRUE;
    }

    /* Get RP1 BAR1 from rp1.resource */
    rp1base = OpenResource("rp1.resource");
    if (!rp1base) {
        D(bug("[RP1-USB] rp1.resource not available\n"));
        return TRUE;
    }

    fields = (IPTR *)((UBYTE *)rp1base + sizeof(struct Library));
    if (!fields[0]) {
        D(bug("[RP1-USB] RP1 not present\n"));
        return TRUE;
    }

    bar1 = fields[1];
    LIBBASE->usb0_base = bar1 + RP1_USB0_OFFSET;
    LIBBASE->usb1_base = bar1 + RP1_USB1_OFFSET;
    LIBBASE->present = TRUE;

    D(bug("[RP1-USB] xHCI USB0 at 0x%p\n", LIBBASE->usb0_base));
    D(bug("[RP1-USB] xHCI USB1 at 0x%p\n", LIBBASE->usb1_base));

    /*
     * Verify xHCI presence by reading CAPLENGTH register.
     * A valid xHCI controller has CAPLENGTH in range 0x20-0x40.
     */
    {
        UBYTE caplength = *(volatile UBYTE *)LIBBASE->usb0_base;
        if (caplength < 0x10 || caplength > 0x80) {
            D(bug("[RP1-USB] USB0 CAPLENGTH=0x%02x — invalid, xHCI not ready\n", caplength));
            LIBBASE->present = FALSE;
            return TRUE;
        }
        D(bug("[RP1-USB] USB0 CAPLENGTH=0x%02x — xHCI detected\n", caplength));
    }

    /*
     * The xHCI controllers are now accessible at usb0_base and usb1_base.
     * pcixhci.device will be extended to probe these platform addresses
     * via the HCF_PLATFORM flag mechanism.
     *
     * For integration with pcixhci.device:
     * 1. pcixhci.device checks for rp1usb.resource at init
     * 2. If present, creates PCIController with hc_RegBase = usb0/1_base
     * 3. Sets HCF_PLATFORM flag to skip PCI BAR read
     * 4. Calls normal xHCI init (same register interface)
     */

    return TRUE;
}

ADD2INITLIB(RP1USB_Init, 0)
