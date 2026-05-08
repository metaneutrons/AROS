#ifndef PCI_BCM2711_H
 * Author: Fabian Schmieder
#define PCI_BCM2711_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <oop/oop.h>

#include LC_LIBDEFS_FILE

/* BCM2711 PCIe Root Complex base address (ARM physical) */
#define PCIE_BASE               0xFD500000
#define PCIE_SIZE               0x9310

/* Register offsets from U-Boot pcie_brcmstb.c */
#define PCIE_MISC_MISC_CTRL                     0x4008
#define PCIE_MISC_RC_BAR1_CONFIG_LO             0x402c
#define PCIE_MISC_RC_BAR2_CONFIG_LO             0x4034
#define PCIE_MISC_RC_BAR2_CONFIG_HI             0x4038
#define PCIE_MISC_RC_BAR3_CONFIG_LO             0x403c
#define PCIE_MISC_PCIE_STATUS                   0x4068
#define PCIE_MISC_HARD_PCIE_HARD_DEBUG          0x4204
#define PCIE_MSI_INTR2_CLR                      0x4508
#define PCIE_MSI_INTR2_MASK_SET                 0x4510

#define PCIE_EXT_CFG_INDEX                      0x9000
#define PCIE_EXT_CFG_DATA                       0x8000

#define PCIE_RGR1_SW_INIT_1                     0x9210

#define PCIE_RC_CFG_PRIV1_ID_VAL3               0x043c
#define PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1  0x0188
#define PCIE_RC_DL_MDIO_ADDR                    0x1100
#define PCIE_RC_DL_MDIO_WR_DATA                 0x1104
#define PCIE_RC_DL_MDIO_RD_DATA                 0x1108

/* PCIe capability registers offset */
#define BRCM_PCIE_CAP_REGS                      0x00ac

/* PCIE_MISC_MISC_CTRL bits */
#define MISC_CTRL_SCB_ACCESS_EN_MASK            (1 << 12)
#define MISC_CTRL_CFG_READ_UR_MODE_MASK         (1 << 13)
#define MISC_CTRL_MAX_BURST_SIZE_MASK           (0x3 << 20)
#define MISC_CTRL_MAX_BURST_SIZE_128            (0x0 << 20)
#define MISC_CTRL_SCB0_SIZE_MASK                (0x1f << 27)

/* PCIE_MISC_PCIE_STATUS bits */
#define STATUS_PCIE_PORT_MASK                   (1 << 7)
#define STATUS_PCIE_PORT_SHIFT                  7
#define STATUS_PCIE_DL_ACTIVE_MASK              (1 << 5)
#define STATUS_PCIE_DL_ACTIVE_SHIFT             5
#define STATUS_PCIE_PHYLINKUP_MASK              (1 << 4)
#define STATUS_PCIE_PHYLINKUP_SHIFT             4

/* PCIE_RGR1_SW_INIT_1 bits */
#define PCIE_RGR1_SW_INIT_1_INIT_MASK           (1 << 0)
#define PCIE_RGR1_SW_INIT_1_PERST_MASK          (1 << 1)

/* PCIE_MISC_HARD_PCIE_HARD_DEBUG bits */
#define PCIE_HARD_DEBUG_SERDES_IDDQ_MASK        (1 << 27)

/* RC_BAR config bits */
#define RC_BAR1_CONFIG_LO_SIZE_MASK             0x1f
#define RC_BAR2_CONFIG_LO_SIZE_MASK             0x1f
#define RC_BAR3_CONFIG_LO_SIZE_MASK             0x1f

/* PCIE_RC_CFG_PRIV1_ID_VAL3 bits */
#define PCIE_RC_CFG_PRIV1_ID_VAL3_CLASS_CODE_MASK 0xffffff

/* Vendor specific register bits */
#define PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BAR2_MASK (0x3 << 0)
#define VENDOR_SPECIFIC_REG1_LITTLE_ENDIAN      0x0

/* Memory window registers */
#define PCIE_MEM_WIN0_LO(win)           (0x400c + (win) * 8)
#define PCIE_MEM_WIN0_HI(win)           (0x4010 + (win) * 8)
#define PCIE_MEM_WIN0_BASE_LIMIT(win)   (0x4070 + (win) * 4)
#define PCIE_MEM_WIN0_BASE_HI(win)      (0x4080 + (win) * 8)
#define PCIE_MEM_WIN0_LIMIT_HI(win)     (0x4084 + (win) * 8)

#define MEM_WIN0_BASE_LIMIT_BASE_MASK   0x0000fff0
#define MEM_WIN0_BASE_LIMIT_LIMIT_MASK  0xfff00000
#define MEM_WIN0_BASE_LIMIT_BASE_HI_SHIFT 12
#define MEM_WIN0_BASE_HI_BASE_MASK      0x000000ff
#define PCIE_MEM_WIN0_LIMIT_HI_LIMIT_MASK 0x000000ff

/* Link capability */
#define LINK_CAPABILITY_ASPM_SUPPORT_MASK (0x3 << 10)

/* ECAM address encoding */
#define PCIE_ECAM_OFFSET(bus, dev, func, reg) \
    (((bus) << 20) | ((dev) << 15) | ((func) << 12) | (reg))

/*
 * PCIe outbound memory window.
 * From DT: ranges = <0x02000000 0x0 0xf8000000 0x6 0x00000000 0x0 0x04000000>
 * CPU address: 0x6_00000000, PCIe address: 0xf8000000, size: 64MB
 * But on RPi4 with 32-bit DMA limitation, the firmware maps it at:
 * CPU 0xc0000000, PCIe 0xf8000000 (via inbound BAR2)
 */
#define PCIE_MEM_CPU_BASE       0x600000000ULL
#define PCIE_MEM_PCIE_BASE      0xf8000000ULL
#define PCIE_MEM_SIZE           0x04000000ULL  /* 64 MB */

struct pci_staticdata {
    OOP_AttrBase        hiddPCIDriverAB;
    OOP_AttrBase        hiddAB;

    OOP_Class           *driverClass;

    IPTR                RegBase;        /* MMIO base of PCIe RC */
    IPTR                CfgBase;        /* Config space access base */
    BOOL                LinkUp;
};

struct pcibase {
    struct Library          LibNode;
    struct pci_staticdata   psd;
};

#define PSD(cl) (&((struct pcibase *)cl->UserData)->psd)

#endif /* PCI_BCM2711_H */
