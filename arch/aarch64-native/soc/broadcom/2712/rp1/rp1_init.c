/*
 * RP1 Southbridge driver for Raspberry Pi 5
 *
 * The RP1 is a PCIe-attached I/O controller that provides:
 * - 2× USB 3.0 (xHCI)
 * - Gigabit Ethernet (GENET-like)
 * - GPIO, I2C, SPI, UART
 *
 * It appears as a PCIe device (vendor 0x1de4, device 0x0001).
 * BAR1 contains all peripheral registers as MMIO offsets.
 *
 * This resource discovers the RP1 via PCI enumeration and exports
 * the BAR1 base address for other drivers to use.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/resident.h>
#include <aros/debug.h>
#include <aros/symbolsets.h>
#include <aros/macros.h>
#include <proto/exec.h>
#include <proto/oop.h>
#include <hidd/pci.h>

#include "rp1.h"

#include LC_LIBDEFS_FILE

/* Global RP1 state — accessible by other drivers */
struct RP1Base *RP1;

static int RP1_Init(LIBBASETYPEPTR LIBBASE)
{
    struct Library *OOPBase;
    OOP_Object *pci;
    OOP_Object *dev = NULL;
    IPTR vendor, device, bar1_addr, bar1_size;

    D(bug("[RP1] Init — scanning PCIe for RP1 southbridge\n"));

    OOPBase = OpenLibrary("oop.library", 0);
    if (!OOPBase)
        return TRUE; /* Not fatal — RPi4 doesn't have RP1 */

    /* Find PCI subsystem */
    pci = OOP_NewObject(NULL, CLID_Hidd_PCI, NULL);
    if (!pci) {
        CloseLibrary(OOPBase);
        return TRUE;
    }

    /*
     * Scan for RP1: Vendor 0x1de4, Device 0x0001.
     * On RPi5, this is the only device on the internal PCIe bus.
     *
     * TODO: Use proper PCI enumeration hook. For now, directly
     * read config space for bus 1, dev 0, func 0.
     */
    {
        struct pHidd_PCIDriver_ReadConfigLong rcl;
        OOP_AttrBase HiddPCIDriverAttrBase = OOP_ObtainAttrBase(IID_Hidd_PCIDriver);

        /* Read vendor/device from bus 1, dev 0 */
        rcl.mID = OOP_GetMethodID(IID_Hidd_PCIDriver, moHidd_PCIDriver_ReadConfigLong);
        rcl.bus = 1;
        rcl.dev = 0;
        rcl.sub = 0;
        rcl.reg = 0x00; /* Vendor + Device ID */

        ULONG id = OOP_DoMethod(pci, (OOP_Msg)&rcl);
        vendor = id & 0xFFFF;
        device = (id >> 16) & 0xFFFF;

        if (vendor == RP1_PCIE_VENDOR_ID && device == RP1_PCIE_DEVICE_ID) {
            /* Read BAR1 */
            rcl.reg = 0x14; /* BAR1 */
            bar1_addr = OOP_DoMethod(pci, (OOP_Msg)&rcl) & ~0xF;

            D(bug("[RP1] Found RP1 at PCIe bus 1 dev 0, BAR1=0x%p\n", bar1_addr));

            LIBBASE->rp1_BAR1 = bar1_addr;
            LIBBASE->rp1_Present = TRUE;

            /* Export peripheral addresses */
            LIBBASE->rp1_USB0 = bar1_addr + RP1_USB0_OFFSET;
            LIBBASE->rp1_USB1 = bar1_addr + RP1_USB1_OFFSET;
            LIBBASE->rp1_ETH  = bar1_addr + RP1_ETH_OFFSET;
            LIBBASE->rp1_GPIO = bar1_addr + RP1_GPIO_OFFSET;
            LIBBASE->rp1_I2C0 = bar1_addr + RP1_I2C0_OFFSET;
            LIBBASE->rp1_I2C1 = bar1_addr + RP1_I2C1_OFFSET;
            LIBBASE->rp1_UART0 = bar1_addr + RP1_UART0_OFFSET;

            D(bug("[RP1] USB0=0x%p ETH=0x%p GPIO=0x%p\n",
                  LIBBASE->rp1_USB0, LIBBASE->rp1_ETH, LIBBASE->rp1_GPIO));
        } else {
            D(bug("[RP1] No RP1 found (vendor=0x%04x device=0x%04x) — RPi4?\n",
                  vendor, device));
        }

        if (HiddPCIDriverAttrBase)
            OOP_ReleaseAttrBase(IID_Hidd_PCIDriver);
    }

    OOP_DisposeObject(pci);
    CloseLibrary(OOPBase);

    RP1 = LIBBASE;

    return TRUE;
}

ADD2INITLIB(RP1_Init, 0)
