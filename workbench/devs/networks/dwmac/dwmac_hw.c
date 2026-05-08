/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Author: Fabian Schmieder
 */
/*
 * Synopsys DesignWare MAC — Hardware access
 *
 * Init, TX, RX, MDIO for the DesignWare Ethernet MAC on RP1 (RPi5).
 * Based on U-Boot drivers/net/designware.c and Linux stmmac.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <aros/macros.h>
#include <aros/debug.h>
#include <proto/exec.h>
#include <devices/sana2.h>
#include <string.h>

#include "dwmac.h"
#include "delay.h"
#define udelay udelay_calibrated

/* Register access */
static inline ULONG dw_rd(struct DWMACUnit *u, ULONG off)
{
    return AROS_LE2LONG(*(volatile ULONG *)(u->du_RegBase + off));
}

static inline void dw_wr(struct DWMACUnit *u, ULONG off, ULONG val)
{
    *(volatile ULONG *)(u->du_RegBase + off) = AROS_LONG2LE(val);
}


/* ============================================================
 * MDIO / PHY
 * ============================================================ */

static BOOL mdio_wait(struct DWMACUnit *unit)
{
    int tries = 10000;
    while ((dw_rd(unit, MAC_MII_ADDR) & MII_BUSY) && --tries)
        udelay(10);
    return tries > 0;
}

UWORD dwmac_mdio_read(struct DWMACUnit *unit, ULONG phy, ULONG reg)
{
    ULONG val;

    if (!mdio_wait(unit))
        return 0xFFFF;

    val = ((phy << MII_ADDR_SHIFT) & 0xF800) |
          ((reg << MII_REG_SHIFT) & 0x07C0) |
          MII_CLK_DIV_62 | MII_BUSY;
    dw_wr(unit, MAC_MII_ADDR, val);

    if (!mdio_wait(unit))
        return 0xFFFF;

    return (UWORD)(dw_rd(unit, MAC_MII_DATA) & 0xFFFF);
}

void dwmac_mdio_write(struct DWMACUnit *unit, ULONG phy, ULONG reg, UWORD data)
{
    if (!mdio_wait(unit))
        return;

    dw_wr(unit, MAC_MII_DATA, data);

    ULONG val = ((phy << MII_ADDR_SHIFT) & 0xF800) |
                ((reg << MII_REG_SHIFT) & 0x07C0) |
                MII_CLK_DIV_62 | MII_WRITE | MII_BUSY;
    dw_wr(unit, MAC_MII_ADDR, val);

    mdio_wait(unit);
}

/* MII register definitions */
#define MII_BMCR    0x00
#define MII_BMSR    0x01
#define MII_ANAR    0x04
#define MII_ANLPAR  0x05
#define MII_GBCR    0x09
#define MII_GBSR    0x0A

#define BMCR_RESET      0x8000
#define BMCR_ANENABLE   0x1000
#define BMCR_ANRESTART  0x0200
#define BMSR_LSTATUS    0x0004

BOOL dwmac_phy_init(struct DWMACUnit *unit)
{
    UWORD bmsr;
    int tries;

    /* Reset PHY */
    dwmac_mdio_write(unit, unit->du_PhyAddr, MII_BMCR, BMCR_RESET);
    tries = 1000;
    while ((dwmac_mdio_read(unit, unit->du_PhyAddr, MII_BMCR) & BMCR_RESET) && --tries)
        udelay(1000);

    /* Start auto-negotiation */
    dwmac_mdio_write(unit, unit->du_PhyAddr, MII_ANAR, 0x01E1); /* 10/100 all modes */
    dwmac_mdio_write(unit, unit->du_PhyAddr, MII_GBCR, 0x0300); /* 1000 full/half */
    dwmac_mdio_write(unit, unit->du_PhyAddr, MII_BMCR, BMCR_ANENABLE | BMCR_ANRESTART);

    /* Wait for link */
    tries = 5000;
    do {
        bmsr = dwmac_mdio_read(unit, unit->du_PhyAddr, MII_BMSR);
        if (bmsr & BMSR_LSTATUS) break;
        udelay(1000);
    } while (--tries);

    unit->du_LinkUp = (bmsr & BMSR_LSTATUS) ? TRUE : FALSE;

    if (unit->du_LinkUp) {
        UWORD gbsr = dwmac_mdio_read(unit, unit->du_PhyAddr, MII_GBSR);
        UWORD anlpar = dwmac_mdio_read(unit, unit->du_PhyAddr, MII_ANLPAR);

        if (gbsr & 0x0C00)
            unit->du_LinkSpeed = 1000;
        else if (anlpar & 0x0180)
            unit->du_LinkSpeed = 100;
        else
            unit->du_LinkSpeed = 10;

        D(bug("[dwmac] Link up at %ldMbps\n", unit->du_LinkSpeed));
    }

    return unit->du_LinkUp;
}

/* ============================================================
 * DMA Init
 * ============================================================ */

void dwmac_hw_init(struct DWMACUnit *unit)
{
    ULONG i, mac_conf;

    D(bug("[dwmac] HW init at 0x%p\n", unit->du_RegBase));

    /* DMA soft reset */
    dw_wr(unit, DMA_BUS_MODE, DMA_BUS_MODE_SWR);
    {
        int tries = 1000;
        while ((dw_rd(unit, DMA_BUS_MODE) & DMA_BUS_MODE_SWR) && --tries)
            udelay(100);
    }

    /* DMA bus mode: fixed burst, PBL=8 */
    dw_wr(unit, DMA_BUS_MODE, DMA_BUS_MODE_FB | DMA_BUS_MODE_PBL(8));

    /* Initialize TX descriptors */
    for (i = 0; i < TX_RING_SIZE; i++) {
        unit->du_TxDesc[i].status = 0;
        unit->du_TxDesc[i].ctrl = DESC_TX_CHAIN;
        unit->du_TxDesc[i].buf_addr = (ULONG)(IPTR)(unit->du_TxBuf + i * ETH_BUF_SIZE);
        unit->du_TxDesc[i].next_desc = (ULONG)(IPTR)&unit->du_TxDesc[(i + 1) % TX_RING_SIZE];
    }

    /* Initialize RX descriptors */
    for (i = 0; i < RX_RING_SIZE; i++) {
        unit->du_RxDesc[i].status = DESC_OWN;
        unit->du_RxDesc[i].ctrl = DESC_RX_CHAIN | (ETH_BUF_SIZE << DESC_CTRL_SIZE1_SHIFT);
        unit->du_RxDesc[i].buf_addr = (ULONG)(IPTR)(unit->du_RxBuf + i * ETH_BUF_SIZE);
        unit->du_RxDesc[i].next_desc = (ULONG)(IPTR)&unit->du_RxDesc[(i + 1) % RX_RING_SIZE];
    }

    /* Set descriptor list addresses */
    dw_wr(unit, DMA_TX_DESC_LIST, (ULONG)(IPTR)unit->du_TxDesc);
    dw_wr(unit, DMA_RX_DESC_LIST, (ULONG)(IPTR)unit->du_RxDesc);

    /* Set MAC address */
    dwmac_hw_set_mac(unit, unit->du_DevAddr);

    /* PHY init */
    dwmac_phy_init(unit);

    /* Configure MAC based on link speed */
    mac_conf = MAC_CONF_DM | MAC_CONF_ACS | MAC_CONF_TE | MAC_CONF_RE;
    if (unit->du_LinkSpeed == 100)
        mac_conf |= MAC_CONF_FES | MAC_CONF_PS;
    else if (unit->du_LinkSpeed == 10)
        mac_conf |= MAC_CONF_PS;
    /* 1000Mbps: no PS, no FES */
    dw_wr(unit, MAC_CONF, mac_conf);

    /* Enable DMA interrupts */
    dw_wr(unit, DMA_INT_ENABLE, DMA_INT_EN_NIE | DMA_INT_EN_RIE | DMA_INT_EN_TIE);

    /* Start DMA TX + RX */
    dw_wr(unit, DMA_OP_MODE, DMA_OP_MODE_SF | DMA_OP_MODE_ST | DMA_OP_MODE_SR);

    /* Kick RX DMA */
    dw_wr(unit, DMA_RX_POLL, 1);

    unit->du_TxCur = 0;
    unit->du_RxCur = 0;

    D(bug("[dwmac] HW init complete\n"));
}

void dwmac_hw_stop(struct DWMACUnit *unit)
{
    /* Disable TX/RX */
    dw_wr(unit, MAC_CONF, dw_rd(unit, MAC_CONF) & ~(MAC_CONF_TE | MAC_CONF_RE));
    /* Stop DMA */
    dw_wr(unit, DMA_OP_MODE, 0);
    /* Disable interrupts */
    dw_wr(unit, DMA_INT_ENABLE, 0);
}

void dwmac_hw_set_mac(struct DWMACUnit *unit, UBYTE *addr)
{
    ULONG hi = (addr[5] << 8) | addr[4];
    ULONG lo = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
    dw_wr(unit, MAC_ADDR_HIGH, hi);
    dw_wr(unit, MAC_ADDR_LOW, lo);
}

/* ============================================================
 * TX / RX
 * ============================================================ */

static void dwmac_dma_reset(struct DWMACUnit *unit);
int dwmac_hw_send(struct DWMACUnit *unit, UBYTE *data, ULONG length)
{
    ULONG idx = unit->du_TxCur;
    struct DWMACDesc *desc = &unit->du_TxDesc[idx];
    int tries = 10000;

    /* Wait for descriptor to be free (with timeout) */
    while ((AROS_LE2LONG(desc->status) & DESC_OWN) && --tries)
        udelay_calibrated(10);

    if (!tries) {
        D(bug("[dwmac] TX timeout — DMA reset\n"));
        dwmac_dma_reset(unit);
        return -1;
    }

    /* Copy data to TX buffer */
    CopyMem(data, unit->du_TxBuf + idx * ETH_BUF_SIZE, length);
    CacheClearE(unit->du_TxBuf + idx * ETH_BUF_SIZE, length, CACRF_ClearD);

    /* Set up descriptor */
    desc->ctrl = AROS_LONG2LE(DESC_TX_CHAIN | (length << DESC_CTRL_SIZE1_SHIFT));
    desc->status = AROS_LONG2LE(DESC_OWN | DESC_TX_INT | DESC_TX_FIRST | DESC_TX_LAST);

    /* Flush descriptor */
    CacheClearE(desc, sizeof(*desc), CACRF_ClearD);

    /* Advance ring */
    unit->du_TxCur = (idx + 1) % TX_RING_SIZE;

    /* Kick TX DMA */
    dw_wr(unit, DMA_TX_POLL, 1);

    return 0;
}

void dwmac_rx_poll(struct DWMACUnit *unit)
{
    while (1) {
        ULONG idx = unit->du_RxCur;
        struct DWMACDesc *desc = &unit->du_RxDesc[idx];
        ULONG status = AROS_LE2LONG(desc->status);

        if (status & DESC_OWN)
            break; /* DMA still owns this descriptor */

        if (!(status & DESC_RX_ERROR)) {
            ULONG length = (status >> DESC_RX_FRAME_LEN_SHIFT) & DESC_RX_FRAME_LEN_MASK;
            UBYTE *pkt = unit->du_RxBuf + idx * ETH_BUF_SIZE;

            /* Deliver to SANA-II read queue */
            if (length >= ETH_HEADERSIZE && length <= ETH_MAXPACKETSIZE) {
                struct IOSana2Req *req;

                ObtainSemaphore(&unit->du_Lock);
                req = (struct IOSana2Req *)RemHead((struct List *)&unit->du_ReadList);
                ReleaseSemaphore(&unit->du_Lock);

                if (req) {
                    CopyMem(pkt, req->ios2_DstAddr, ETH_ADDRSIZE);
                    CopyMem(pkt + ETH_ADDRSIZE, req->ios2_SrcAddr, ETH_ADDRSIZE);
                    req->ios2_PacketType = (pkt[12] << 8) | pkt[13];
                    req->ios2_DataLength = length - ETH_HEADERSIZE;
                    CopyMem(pkt + ETH_HEADERSIZE, req->ios2_Data, req->ios2_DataLength);
                    req->ios2_Req.io_Error = 0;
                    ReplyMsg((struct Message *)req);
                    unit->du_Stats.PacketsReceived++;
                }
            }
        }

        /* Return descriptor to DMA */
        desc->status = AROS_LONG2LE(DESC_OWN);
        CacheClearE(desc, sizeof(*desc), CACRF_ClearD);

        unit->du_RxCur = (idx + 1) % RX_RING_SIZE;
    }

    /* Kick RX DMA in case it stalled */
    dw_wr(unit, DMA_RX_POLL, 1);
}

/* ============================================================
 * IRQ Handler
 * ============================================================ */

void dwmac_irq_handler(struct DWMACUnit *unit, void *data2)
{
    struct ExecBase *SysBase = (struct ExecBase *)data2;
    ULONG status = AROS_LE2LONG(*(volatile ULONG *)(unit->du_RegBase + DMA_STATUS));

    /* Acknowledge all interrupts */
    *(volatile ULONG *)(unit->du_RegBase + DMA_STATUS) = AROS_LONG2LE(status);

    if (unit->du_Task && unit->du_IntSig != (ULONG)-1)
        Signal(unit->du_Task, 1 << unit->du_IntSig);
}

/* ============================================================
 * Error Recovery
 * ============================================================ */

/*
 * Reset the DMA engine after a timeout or error.
 * Preserves MAC configuration but reinitializes DMA rings.
 */
static void dwmac_dma_reset(struct DWMACUnit *unit)
{
    ULONG i;

    D(bug("[dwmac] DMA reset\n"));

    /* Stop DMA */
    dw_wr(unit, DMA_OP_MODE, 0);
    udelay_calibrated(100);

    /* Soft reset DMA */
    dw_wr(unit, DMA_BUS_MODE, DMA_BUS_MODE_SWR);
    {
        int tries = 100;
        while ((dw_rd(unit, DMA_BUS_MODE) & DMA_BUS_MODE_SWR) && --tries)
            udelay_calibrated(100);
        if (!tries)
            D(bug("[dwmac] DMA reset timeout!\n"));
    }

    /* Reinitialize DMA */
    dw_wr(unit, DMA_BUS_MODE, DMA_BUS_MODE_FB | DMA_BUS_MODE_PBL(8));

    /* Reset descriptor rings */
    for (i = 0; i < TX_RING_SIZE; i++) {
        unit->du_TxDesc[i].status = 0;
        unit->du_TxDesc[i].ctrl = AROS_LONG2LE(DESC_TX_CHAIN);
    }
    for (i = 0; i < RX_RING_SIZE; i++) {
        unit->du_RxDesc[i].status = AROS_LONG2LE(DESC_OWN);
        unit->du_RxDesc[i].ctrl = AROS_LONG2LE(DESC_RX_CHAIN | (ETH_BUF_SIZE << DESC_CTRL_SIZE1_SHIFT));
    }

    dw_wr(unit, DMA_TX_DESC_LIST, (ULONG)(IPTR)unit->du_TxDesc);
    dw_wr(unit, DMA_RX_DESC_LIST, (ULONG)(IPTR)unit->du_RxDesc);

    unit->du_TxCur = 0;
    unit->du_RxCur = 0;

    /* Re-enable DMA */
    dw_wr(unit, DMA_INT_ENABLE, DMA_INT_EN_NIE | DMA_INT_EN_RIE | DMA_INT_EN_TIE);
    dw_wr(unit, DMA_OP_MODE, DMA_OP_MODE_SF | DMA_OP_MODE_ST | DMA_OP_MODE_SR);
    dw_wr(unit, DMA_RX_POLL, 1);

    unit->du_Stats.Overruns++;
}

/*
 * Check for PHY link-down and attempt recovery.
 * Called periodically from the unit task.
 */
void dwmac_check_link(struct DWMACUnit *unit)
{
    UWORD bmsr = dwmac_mdio_read(unit, unit->du_PhyAddr, 0x01); /* BMSR */
    BOOL link_now = (bmsr & 0x0004) ? TRUE : FALSE;

    if (unit->du_LinkUp && !link_now) {
        D(bug("[dwmac] Link DOWN\n"));
        unit->du_LinkUp = FALSE;
        /* Disable TX/RX until link returns */
        dw_wr(unit, MAC_CONF, dw_rd(unit, MAC_CONF) & ~(MAC_CONF_TE | MAC_CONF_RE));
    } else if (!unit->du_LinkUp && link_now) {
        D(bug("[dwmac] Link UP — renegotiating\n"));
        dwmac_phy_init(unit);
        /* Re-enable TX/RX with new speed */
        ULONG mac_conf = MAC_CONF_DM | MAC_CONF_ACS | MAC_CONF_TE | MAC_CONF_RE;
        if (unit->du_LinkSpeed == 100)
            mac_conf |= MAC_CONF_FES | MAC_CONF_PS;
        else if (unit->du_LinkSpeed == 10)
            mac_conf |= MAC_CONF_PS;
        dw_wr(unit, MAC_CONF, mac_conf);
    }
}
