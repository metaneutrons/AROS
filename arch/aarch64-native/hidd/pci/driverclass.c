/*
 * BCM2711 PCIe Root Complex — PCI HIDD Driver Class
 *
 * Provides config space read/write for the BCM2711 PCIe controller
 * as found on the Raspberry Pi 4. Based on U-Boot pcie_brcmstb.c.
 *
 * The VL805 xHCI USB 3.0 controller is the only device on this bus.
 */

#define __OOP_NOATTRBASES__

#include <exec/types.h>
#include <hidd/pci.h>
#include <oop/oop.h>
#include <utility/tagitem.h>

#include <proto/exec.h>
#include <proto/oop.h>
#include <proto/kernel.h>

#include <aros/symbolsets.h>
#include <aros/macros.h>

#include "pci.h"

#define DEBUG 1
#include <aros/debug.h>

#undef HiddPCIDriverAttrBase
#undef HiddAttrBase

#define HiddPCIDriverAttrBase   (PSD(cl)->hiddPCIDriverAB)
#define HiddAttrBase            (PSD(cl)->hiddAB)

/* Register access */
static inline ULONG pcie_rd(struct pci_staticdata *psd, ULONG off)
{
    return AROS_LE2LONG(*(volatile ULONG *)(psd->RegBase + off));
}

static inline void pcie_wr(struct pci_staticdata *psd, ULONG off, ULONG val)
{
    *(volatile ULONG *)(psd->RegBase + off) = AROS_LONG2LE(val);
}

static inline void pcie_set(struct pci_staticdata *psd, ULONG off, ULONG bits)
{
    pcie_wr(psd, off, pcie_rd(psd, off) | bits);
}

static inline void pcie_clr(struct pci_staticdata *psd, ULONG off, ULONG bits)
{
    pcie_wr(psd, off, pcie_rd(psd, off) & ~bits);
}

static BOOL pcie_link_up(struct pci_staticdata *psd)
{
    ULONG val = pcie_rd(psd, PCIE_MISC_PCIE_STATUS);
    ULONG dla = (val & STATUS_PCIE_DL_ACTIVE_MASK) >> STATUS_PCIE_DL_ACTIVE_SHIFT;
    ULONG plu = (val & STATUS_PCIE_PHYLINKUP_MASK) >> STATUS_PCIE_PHYLINKUP_SHIFT;
    return dla && plu;
}

/*
 * New method — announce ourselves as the BCM2711 PCIe driver.
 */
OOP_Object *PCIBrcm__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
    struct pRoot_New mymsg;

    struct TagItem mytags[] = {
        { aHidd_Name,         (IPTR)"PCIBrcm" },
        { aHidd_HardwareName, (IPTR)"BCM2711 PCIe Root Complex" },
        { TAG_DONE, 0 }
    };

    mymsg.mID = msg->mID;
    mymsg.attrList = mytags;

    if (msg->attrList) {
        mytags[2].ti_Tag = TAG_MORE;
        mytags[2].ti_Data = (IPTR)msg->attrList;
    }

    msg = &mymsg;
    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);

    return o;
}

/*
 * ReadConfigLong — read a 32-bit value from PCI config space.
 *
 * Bus 0 = RC itself (direct register access).
 * Bus 1+ = downstream devices via ECAM index/data registers.
 */
ULONG PCIBrcm__Hidd_PCIDriver__ReadConfigLong(OOP_Class *cl, OOP_Object *o,
                                               struct pHidd_PCIDriver_ReadConfigLong *msg)
{
    struct pci_staticdata *psd = PSD(cl);
    UBYTE bus  = msg->bus;
    UBYTE dev  = msg->dev;
    UBYTE sub  = msg->sub;
    UWORD reg  = msg->reg;

    /* Buses 0 and 1 are limited to device 0 */
    if (bus < 2 && dev > 0)
        return 0xFFFFFFFF;

    /* Bus 0 = Root Complex registers */
    if (bus == 0) {
        return pcie_rd(psd, reg);
    }

    /* No link = no downstream access */
    if (!psd->LinkUp)
        return 0xFFFFFFFF;

    /* Write ECAM index for the target BDF */
    pcie_wr(psd, PCIE_EXT_CFG_INDEX, PCIE_ECAM_OFFSET(bus, dev, sub, 0));

    /* Read from ECAM data window */
    return AROS_LE2LONG(*(volatile ULONG *)(psd->RegBase + PCIE_EXT_CFG_DATA + reg));
}

/*
 * WriteConfigLong — write a 32-bit value to PCI config space.
 */
void PCIBrcm__Hidd_PCIDriver__WriteConfigLong(OOP_Class *cl, OOP_Object *o,
                                               struct pHidd_PCIDriver_WriteConfigLong *msg)
{
    struct pci_staticdata *psd = PSD(cl);
    UBYTE bus  = msg->bus;
    UBYTE dev  = msg->dev;
    UBYTE sub  = msg->sub;
    UWORD reg  = msg->reg;
    ULONG val  = msg->val;

    if (bus < 2 && dev > 0)
        return;

    if (bus == 0) {
        pcie_wr(psd, reg, val);
        return;
    }

    if (!psd->LinkUp)
        return;

    pcie_wr(psd, PCIE_EXT_CFG_INDEX, PCIE_ECAM_OFFSET(bus, dev, sub, 0));
    *(volatile ULONG *)(psd->RegBase + PCIE_EXT_CFG_DATA + reg) = AROS_LONG2LE(val);
}
