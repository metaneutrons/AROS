/*
 * RP1 USB 3.0 (xHCI) initialization for Raspberry Pi 5
 *
 * The RP1 southbridge contains two xHCI USB 3.0 controllers at
 * BAR1 + 0x100000 (USB0) and BAR1 + 0x110000 (USB1).
 * These are NOT separate PCI devices — they're MMIO regions within
 * the RP1's BAR1 space.
 *
 * This resource exports the xHCI base addresses so that a modified
 * xHCI driver (or Poseidon) can access them directly.
 *
 * On RPi4, USB 3.0 is via VL805 (a real PCIe xHCI device) and is
 * handled by pcixhci.device through normal PCI enumeration.
 */

#include <exec/types.h>
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
    IPTR            usb0_base;  /* xHCI registers for USB0 */
    IPTR            usb1_base;  /* xHCI registers for USB1 */
    BOOL            present;
};

APTR KernelBase = NULL;

static int RP1USB_Init(LIBBASETYPEPTR LIBBASE)
{
    struct Library *rp1base;
    IPTR *fields;

    D(bug("[RP1-USB] Init\n"));

    KernelBase = OpenResource("kernel.resource");
    if (!KernelBase)
        return TRUE;

    /* Only active on RPi5 */
    if (KrnGetSystemAttr(KATTR_PeripheralBase) == 0xFE000000) {
        D(bug("[RP1-USB] RPi4 — not needed (VL805 via PCIe)\n"));
        return TRUE;
    }

    /* Get RP1 BAR1 from rp1.resource */
    rp1base = OpenResource("rp1.resource");
    if (!rp1base) {
        D(bug("[RP1-USB] rp1.resource not available\n"));
        return TRUE;
    }

    /* Read BAR1 and present flag from RP1Base struct */
    fields = (IPTR *)((UBYTE *)rp1base + sizeof(struct Library));
    if (!fields[0]) { /* rp1_Present */
        D(bug("[RP1-USB] RP1 not present\n"));
        return TRUE;
    }

    IPTR bar1 = fields[1]; /* rp1_BAR1 */

    LIBBASE->usb0_base = bar1 + RP1_USB0_OFFSET;
    LIBBASE->usb1_base = bar1 + RP1_USB1_OFFSET;
    LIBBASE->present = TRUE;

    D(bug("[RP1-USB] USB0=0x%p USB1=0x%p\n",
          LIBBASE->usb0_base, LIBBASE->usb1_base));

    /*
     * TODO: Initialize the xHCI controllers here or signal
     * Poseidon/pcixhci to probe these addresses.
     * For now, export the addresses for manual probing.
     */

    return TRUE;
}

ADD2INITLIB(RP1USB_Init, 0)
