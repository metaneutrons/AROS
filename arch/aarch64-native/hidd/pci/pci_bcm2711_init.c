/*
 * BCM2711 PCIe Root Complex — Initialization
 *
 * Performs the hardware bring-up sequence for the BCM2711 PCIe RC:
 * 1. Assert/deassert bridge reset
 * 2. Enable SerDes
 * 3. Configure inbound BAR2 for DMA
 * 4. Deassert PERST# (fundamental reset to endpoint)
 * 5. Wait for link-up
 * 6. Configure outbound memory window
 * 7. Set bridge class code
 * 8. Register as PCI HIDD driver
 */

#include <aros/debug.h>
#include <aros/symbolsets.h>
#include <aros/macros.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/kernel.h>
#include <proto/oop.h>
#include <hidd/pci.h>

#include "pci.h"

#include LC_LIBDEFS_FILE

extern APTR KernelBase;

static inline ULONG pcie_rd(struct pci_staticdata *psd, ULONG off)
{
    return AROS_LE2LONG(*(volatile ULONG *)(psd->RegBase + off));
}

static inline void pcie_wr(struct pci_staticdata *psd, ULONG off, ULONG val)
{
    *(volatile ULONG *)(psd->RegBase + off) = AROS_LONG2LE(val);
}

static void udelay(ULONG us)
{
    volatile ULONG i;
    for (i = 0; i < us * 10; i++)
        ;
}

static void mdelay(ULONG ms)
{
    udelay(ms * 1000);
}

/*
 * Encode inbound BAR size to the non-linear SIZE field value.
 */
static ULONG encode_ibar_size(ULONG size_log2)
{
    if (size_log2 >= 12 && size_log2 <= 15)
        return (size_log2 - 12) + 0x1c;
    else if (size_log2 >= 16 && size_log2 <= 37)
        return size_log2 - 15;
    return 0;
}

static int pcie_hw_init(struct pci_staticdata *psd)
{
    IPTR base = psd->RegBase;
    ULONG tmp;
    int i;

    D(bug("[PCIe] Initializing BCM2711 PCIe RC at 0x%p\n", base));

    /* Assert bridge reset + PERST# */
    tmp = pcie_rd(psd, PCIE_RGR1_SW_INIT_1);
    tmp |= PCIE_RGR1_SW_INIT_1_INIT_MASK | PCIE_RGR1_SW_INIT_1_PERST_MASK;
    pcie_wr(psd, PCIE_RGR1_SW_INIT_1, tmp);
    udelay(200);

    /* Deassert bridge reset (keep PERST# asserted) */
    tmp &= ~PCIE_RGR1_SW_INIT_1_INIT_MASK;
    pcie_wr(psd, PCIE_RGR1_SW_INIT_1, tmp);

    /* Enable SerDes */
    tmp = pcie_rd(psd, PCIE_MISC_HARD_PCIE_HARD_DEBUG);
    tmp &= ~PCIE_HARD_DEBUG_SERDES_IDDQ_MASK;
    pcie_wr(psd, PCIE_MISC_HARD_PCIE_HARD_DEBUG, tmp);
    udelay(200);

    /* Set SCB_MAX_BURST_SIZE, CFG_READ_UR_MODE, SCB_ACCESS_EN */
    tmp = pcie_rd(psd, PCIE_MISC_MISC_CTRL);
    tmp &= ~MISC_CTRL_MAX_BURST_SIZE_MASK;
    tmp |= MISC_CTRL_SCB_ACCESS_EN_MASK | MISC_CTRL_CFG_READ_UR_MODE_MASK |
           MISC_CTRL_MAX_BURST_SIZE_128;
    pcie_wr(psd, PCIE_MISC_MISC_CTRL, tmp);

    /*
     * Configure inbound BAR2 for DMA from endpoint to system RAM.
     * Map the full low 4GB (size = 30 bits = 1GB, offset = 0).
     * The DMA ranges in DT say: PCIe 0xc0000000 → ARM 0x00000000, size 1GB.
     */
    pcie_wr(psd, PCIE_MISC_RC_BAR2_CONFIG_LO, encode_ibar_size(30));
    pcie_wr(psd, PCIE_MISC_RC_BAR2_CONFIG_HI, 0);

    /* Set SCB0 size to match (30 - 15 = 15 = 0xf) */
    tmp = pcie_rd(psd, PCIE_MISC_MISC_CTRL);
    tmp &= ~MISC_CTRL_SCB0_SIZE_MASK;
    tmp |= (0xfUL << 27);
    pcie_wr(psd, PCIE_MISC_MISC_CTRL, tmp);

    /* Disable BAR1 and BAR3 */
    tmp = pcie_rd(psd, PCIE_MISC_RC_BAR1_CONFIG_LO);
    tmp &= ~RC_BAR1_CONFIG_LO_SIZE_MASK;
    pcie_wr(psd, PCIE_MISC_RC_BAR1_CONFIG_LO, tmp);

    tmp = pcie_rd(psd, PCIE_MISC_RC_BAR3_CONFIG_LO);
    tmp &= ~RC_BAR3_CONFIG_LO_SIZE_MASK;
    pcie_wr(psd, PCIE_MISC_RC_BAR3_CONFIG_LO, tmp);

    /* Mask all MSI interrupts */
    pcie_wr(psd, PCIE_MSI_INTR2_MASK_SET, 0xFFFFFFFF);
    pcie_wr(psd, PCIE_MSI_INTR2_CLR, 0xFFFFFFFF);

    /* Deassert PERST# — endpoint can now initialize */
    tmp = pcie_rd(psd, PCIE_RGR1_SW_INIT_1);
    tmp &= ~PCIE_RGR1_SW_INIT_1_PERST_MASK;
    pcie_wr(psd, PCIE_RGR1_SW_INIT_1, tmp);

    /* Wait 100ms for link training (PCIe CEM spec 2.2) */
    mdelay(100);

    /* Poll for link-up, up to 200ms */
    for (i = 0; i < 200; i += 5) {
        if (pcie_rd(psd, PCIE_MISC_PCIE_STATUS) &
            (STATUS_PCIE_DL_ACTIVE_MASK | STATUS_PCIE_PHYLINKUP_MASK)) {
            psd->LinkUp = TRUE;
            break;
        }
        mdelay(5);
    }

    if (!psd->LinkUp) {
        D(bug("[PCIe] Link down — no endpoint detected\n"));
        return 0; /* Not fatal — just no USB3 */
    }

    D(bug("[PCIe] Link up!\n"));

    /* Set class code to PCI-PCI bridge (0x0604) */
    tmp = pcie_rd(psd, PCIE_RC_CFG_PRIV1_ID_VAL3);
    tmp &= ~PCIE_RC_CFG_PRIV1_ID_VAL3_CLASS_CODE_MASK;
    tmp |= 0x060400;
    pcie_wr(psd, PCIE_RC_CFG_PRIV1_ID_VAL3, tmp);

    /* Set BAR2 endianness to little-endian */
    tmp = pcie_rd(psd, PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1);
    tmp &= ~PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BAR2_MASK;
    tmp |= VENDOR_SPECIFIC_REG1_LITTLE_ENDIAN;
    pcie_wr(psd, PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1, tmp);

    /* Disable ASPM (not useful without OS power management) */
    tmp = pcie_rd(psd, BRCM_PCIE_CAP_REGS + 0x0C); /* LNKCAP */
    tmp &= ~LINK_CAPABILITY_ASPM_SUPPORT_MASK;
    pcie_wr(psd, BRCM_PCIE_CAP_REGS + 0x0C, tmp);

    /*
     * Configure outbound memory window 0.
     * CPU 0x6_00000000 → PCIe 0xF8000000, 64MB.
     * This is where the VL805 BARs will be mapped.
     */
    pcie_wr(psd, PCIE_MEM_WIN0_LO(0), (ULONG)(PCIE_MEM_PCIE_BASE & 0xFFFFFFFF));
    pcie_wr(psd, PCIE_MEM_WIN0_HI(0), (ULONG)(PCIE_MEM_PCIE_BASE >> 32));

    D(bug("[PCIe] BCM2711 PCIe RC initialized, link up\n"));

    return 1;
}

static int PCIBrcm_Init(LIBBASETYPEPTR LIBBASE)
{
    struct pci_staticdata *psd = &LIBBASE->psd;

    D(bug("[PCIe] Init\n"));

    KernelBase = OpenResource("kernel.resource");
    if (!KernelBase)
        return FALSE;

    psd->RegBase = PCIE_BASE;
    psd->LinkUp = FALSE;

    pcie_hw_init(psd);

    if (!psd->LinkUp) {
        D(bug("[PCIe] No link — driver inactive\n"));
        /* Still return TRUE so the HIDD is registered;
         * config reads will return 0xFFFFFFFF for absent devices */
    }

    return TRUE;
}

APTR KernelBase = NULL;

ADD2INITLIB(PCIBrcm_Init, 0)
