#ifndef DWMAC_H
#define DWMAC_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/semaphores.h>
#include <exec/devices.h>
#include <devices/sana2.h>

/*
 * Synopsys DesignWare Ethernet MAC driver for RP1 (Raspberry Pi 5)
 *
 * Register definitions from U-Boot drivers/net/designware.h
 * DMA and IRQ handling from Linux drivers/net/ethernet/stmicro/stmmac/
 */

#define DWMAC_DEVICE_NAME   "dwmac.device"

/* Descriptor ring sizes */
#define TX_RING_SIZE        16
#define RX_RING_SIZE        16
#define ETH_BUF_SIZE        2048

/* Network constants */
#define ETH_ADDRSIZE        6
#define ETH_MTU             1500
#define ETH_HEADERSIZE      14
#define ETH_MAXPACKETSIZE   (ETH_MTU + ETH_HEADERSIZE + 4)

/* ============================================================
 * MAC registers (offset 0x0000 from base)
 * ============================================================ */
#define MAC_CONF            0x0000
#define MAC_FRAME_FILTER    0x0004
#define MAC_HASH_HIGH       0x0008
#define MAC_HASH_LOW        0x000C
#define MAC_MII_ADDR        0x0010
#define MAC_MII_DATA        0x0014
#define MAC_FLOW_CTRL       0x0018
#define MAC_VLAN_TAG        0x001C
#define MAC_VERSION         0x0020
#define MAC_INT_STATUS      0x0038
#define MAC_INT_MASK        0x003C
#define MAC_ADDR_HIGH       0x0040
#define MAC_ADDR_LOW        0x0044

/* MAC_CONF bits */
#define MAC_CONF_JD         (1 << 22)   /* Jabber disable */
#define MAC_CONF_BE         (1 << 21)   /* Frame burst enable */
#define MAC_CONF_PS         (1 << 15)   /* Port select (MII) */
#define MAC_CONF_FES        (1 << 14)   /* Speed 100Mbps */
#define MAC_CONF_DM         (1 << 11)   /* Full duplex */
#define MAC_CONF_IPC        (1 << 10)   /* Checksum offload */
#define MAC_CONF_ACS        (1 << 7)    /* Auto pad/CRC strip */
#define MAC_CONF_TE         (1 << 3)    /* TX enable */
#define MAC_CONF_RE         (1 << 2)    /* RX enable */

/* MII address register bits */
#define MII_BUSY            (1 << 0)
#define MII_WRITE           (1 << 1)
#define MII_CLK_DIV_62      (1 << 2)    /* CSR clock 100-150MHz */
#define MII_ADDR_SHIFT      11
#define MII_REG_SHIFT       6

/* ============================================================
 * DMA registers (offset 0x1000 from base)
 * ============================================================ */
#define DMA_BASE            0x1000
#define DMA_BUS_MODE        (DMA_BASE + 0x00)
#define DMA_TX_POLL         (DMA_BASE + 0x04)
#define DMA_RX_POLL         (DMA_BASE + 0x08)
#define DMA_RX_DESC_LIST    (DMA_BASE + 0x0C)
#define DMA_TX_DESC_LIST    (DMA_BASE + 0x10)
#define DMA_STATUS          (DMA_BASE + 0x14)
#define DMA_OP_MODE         (DMA_BASE + 0x18)
#define DMA_INT_ENABLE      (DMA_BASE + 0x1C)

/* DMA_BUS_MODE bits */
#define DMA_BUS_MODE_SWR    (1 << 0)    /* Software reset */
#define DMA_BUS_MODE_FB     (1 << 16)   /* Fixed burst */
#define DMA_BUS_MODE_PBL(x) (((x) & 0x3F) << 8)  /* Burst length */

/* DMA_OP_MODE bits */
#define DMA_OP_MODE_SF      (1 << 21)   /* Store and forward */
#define DMA_OP_MODE_FTF     (1 << 20)   /* Flush TX FIFO */
#define DMA_OP_MODE_ST      (1 << 13)   /* Start TX */
#define DMA_OP_MODE_SR      (1 << 1)    /* Start RX */

/* DMA_STATUS bits */
#define DMA_STATUS_NIS      (1 << 16)   /* Normal interrupt summary */
#define DMA_STATUS_AIS      (1 << 15)   /* Abnormal interrupt summary */
#define DMA_STATUS_RI       (1 << 6)    /* Receive interrupt */
#define DMA_STATUS_TI       (1 << 0)    /* Transmit interrupt */

/* DMA_INT_ENABLE bits */
#define DMA_INT_EN_NIE      (1 << 16)   /* Normal interrupt enable */
#define DMA_INT_EN_RIE      (1 << 6)    /* Receive interrupt enable */
#define DMA_INT_EN_TIE      (1 << 0)    /* Transmit interrupt enable */

/* ============================================================
 * DMA Descriptor (normal format)
 * ============================================================ */
struct DWMACDesc {
    ULONG status;       /* OWN bit + status */
    ULONG ctrl;         /* Buffer size + control */
    ULONG buf_addr;     /* Buffer physical address */
    ULONG next_desc;    /* Next descriptor address */
};

/* Descriptor status bits */
#define DESC_OWN            (1U << 31)  /* Owned by DMA */
#define DESC_TX_INT         (1 << 30)   /* TX: interrupt on completion */
#define DESC_TX_LAST        (1 << 29)   /* TX: last segment */
#define DESC_TX_FIRST       (1 << 28)   /* TX: first segment */
#define DESC_TX_CRC_DIS     (1 << 26)   /* TX: disable CRC */
#define DESC_TX_END_RING    (1 << 25)   /* TX: end of ring */
#define DESC_TX_CHAIN       (1 << 24)   /* TX: chained descriptor */

#define DESC_RX_INT_DIS     (1U << 31)  /* RX: disable interrupt */
#define DESC_RX_END_RING    (1 << 25)   /* RX: end of ring */
#define DESC_RX_CHAIN       (1 << 24)   /* RX: chained descriptor */

#define DESC_RX_FRAME_LEN_SHIFT 16
#define DESC_RX_FRAME_LEN_MASK  0x3FFF
#define DESC_RX_ERROR       (1 << 15)

#define DESC_CTRL_SIZE1_MASK    0x7FF
#define DESC_CTRL_SIZE1_SHIFT   0

/* ============================================================
 * Driver structures
 * ============================================================ */

struct DWMACUnit {
    struct Node         du_Node;
    ULONG               du_UnitNum;
    ULONG               du_Flags;

    struct DWMACBase    *du_Device;
    IPTR                du_RegBase;     /* MMIO base */

    /* MAC address */
    UBYTE               du_DevAddr[ETH_ADDRSIZE];
    UBYTE               du_OrgAddr[ETH_ADDRSIZE];

    /* DMA descriptors (16-byte aligned) */
    struct DWMACDesc    *du_TxDesc;
    struct DWMACDesc    *du_RxDesc;
    ULONG               du_TxCur;
    ULONG               du_RxCur;

    /* Buffers */
    UBYTE               *du_TxBuf;      /* TX_RING_SIZE * ETH_BUF_SIZE */
    UBYTE               *du_RxBuf;      /* RX_RING_SIZE * ETH_BUF_SIZE */

    /* Task and signals */
    struct Task         *du_Task;
    ULONG               du_IntSig;
    APTR                du_IRQHandle;

    /* SANA-II request queues */
    struct MinList      du_ReadList;
    struct MinList      du_WriteList;
    struct SignalSemaphore du_Lock;

    /* Statistics */
    struct Sana2DeviceStats du_Stats;

    /* PHY */
    ULONG               du_PhyAddr;
    ULONG               du_LinkSpeed;
    BOOL                du_LinkUp;
};

#define DUF_ONLINE      (1 << 0)
#define DUF_CONFIGURED  (1 << 1)

struct DWMACBase {
    struct Device       du_Device;
    struct DWMACUnit    *du_Units[1];
    ULONG               du_UnitCount;
    struct SignalSemaphore du_Lock;
};

/* Hardware functions */
void dwmac_hw_init(struct DWMACUnit *unit);
void dwmac_hw_stop(struct DWMACUnit *unit);
int  dwmac_hw_send(struct DWMACUnit *unit, UBYTE *data, ULONG length);
void dwmac_hw_set_mac(struct DWMACUnit *unit, UBYTE *addr);
void dwmac_rx_poll(struct DWMACUnit *unit);
void dwmac_tx_process(struct DWMACUnit *unit);

/* MDIO/PHY */
UWORD dwmac_mdio_read(struct DWMACUnit *unit, ULONG phy, ULONG reg);
void  dwmac_mdio_write(struct DWMACUnit *unit, ULONG phy, ULONG reg, UWORD val);
BOOL  dwmac_phy_init(struct DWMACUnit *unit);

/* IRQ handler */
void dwmac_irq_handler(struct DWMACUnit *unit, void *data2);

#endif /* DWMAC_H */
