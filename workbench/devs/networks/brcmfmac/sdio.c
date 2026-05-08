/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Author: Fabian Schmieder
 */
/*
 * BCM43455 WiFi — SDIO transport layer
 *
 * Implements CMD52 (single-byte) and CMD53 (multi-byte) SDIO I/O
 * via the Arasan SDHCI controller on BCM2711.
 */

#include <exec/types.h>
#include <aros/macros.h>
#include <aros/debug.h>

#include "brcmfmac.h"

/* Register access */
static inline ULONG sdhci_rd(struct BrcmfUnit *unit, ULONG off)
{
    return AROS_LE2LONG(*(volatile ULONG *)(unit->bn_SdhciBase + off));
}

static inline void sdhci_wr(struct BrcmfUnit *unit, ULONG off, ULONG val)
{
    *(volatile ULONG *)(unit->bn_SdhciBase + off) = AROS_LONG2LE(val);
}

static inline UWORD sdhci_rd16(struct BrcmfUnit *unit, ULONG off)
{
    return AROS_LE2WORD(*(volatile UWORD *)(unit->bn_SdhciBase + off));
}

static inline void sdhci_wr16(struct BrcmfUnit *unit, ULONG off, UWORD val)
{
    *(volatile UWORD *)(unit->bn_SdhciBase + off) = AROS_WORD2LE(val);
}

static inline void sdhci_wr8(struct BrcmfUnit *unit, ULONG off, UBYTE val)
{
    *(volatile UBYTE *)(unit->bn_SdhciBase + off) = val;
}

static void udelay(ULONG us)
{
    volatile ULONG i;
    for (i = 0; i < us * 10; i++) ;
}

/*
 * Wait for command inhibit to clear.
 */
static BOOL sdhci_wait_cmd(struct BrcmfUnit *unit)
{
    int tries = 100000;
    while ((sdhci_rd(unit, SDHCI_PRESENT_STATE) & SDHCI_CMD_INHIBIT) && --tries)
        ;
    return tries > 0;
}

/*
 * Wait for command complete interrupt.
 */
static BOOL sdhci_wait_complete(struct BrcmfUnit *unit)
{
    int tries = 100000;
    ULONG status;

    while (--tries) {
        status = sdhci_rd(unit, SDHCI_INT_STATUS);
        if (status & (SDHCI_INT_CMD_COMPLETE | SDHCI_INT_ERROR))
            break;
    }

    /* Clear interrupt */
    sdhci_wr(unit, SDHCI_INT_STATUS, status);

    if (!tries || (status & SDHCI_INT_ERROR))
        return FALSE;

    return TRUE;
}

/*
 * Wait for data transfer complete.
 */
static BOOL sdhci_wait_data(struct BrcmfUnit *unit)
{
    int tries = 1000000;
    ULONG status;

    while (--tries) {
        status = sdhci_rd(unit, SDHCI_INT_STATUS);
        if (status & (SDHCI_INT_XFER_COMPLETE | SDHCI_INT_ERROR))
            break;
    }

    sdhci_wr(unit, SDHCI_INT_STATUS, status);

    if (!tries || (status & SDHCI_INT_ERROR))
        return FALSE;

    return TRUE;
}

/*
 * Send an SDIO command and get the response.
 */
static BOOL sdhci_send_cmd(struct BrcmfUnit *unit, UWORD cmd, ULONG arg, ULONG *resp)
{
    if (!sdhci_wait_cmd(unit))
        return FALSE;

    /* Clear all interrupts */
    sdhci_wr(unit, SDHCI_INT_STATUS, 0xFFFFFFFF);

    /* Set argument */
    sdhci_wr(unit, SDHCI_ARGUMENT, arg);

    /* Send command */
    sdhci_wr16(unit, SDHCI_COMMAND, cmd);

    /* Wait for completion */
    if (!sdhci_wait_complete(unit))
        return FALSE;

    if (resp)
        *resp = sdhci_rd(unit, SDHCI_RESPONSE0);

    return TRUE;
}

/*
 * CMD52: SDIO single-byte read.
 */
UBYTE sdio_cmd52_read(struct BrcmfUnit *unit, ULONG func, ULONG addr)
{
    ULONG arg = SDIO_CMD52_FUNC(func) | SDIO_CMD52_ADDR(addr);
    ULONG resp = 0;
    UWORD cmd = (SDIO_CMD_RW_DIRECT << 8) | SDHCI_CMD_RESP_48 |
                SDHCI_CMD_CRC_CHECK | SDHCI_CMD_INDEX_CHECK;

    if (!sdhci_send_cmd(unit, cmd, arg, &resp))
        return 0xFF;

    return (UBYTE)(resp & 0xFF);
}

/*
 * CMD52: SDIO single-byte write.
 */
void sdio_cmd52_write(struct BrcmfUnit *unit, ULONG func, ULONG addr, UBYTE val)
{
    ULONG arg = SDIO_CMD52_WRITE | SDIO_CMD52_FUNC(func) |
                SDIO_CMD52_ADDR(addr) | SDIO_CMD52_DATA(val);
    UWORD cmd = (SDIO_CMD_RW_DIRECT << 8) | SDHCI_CMD_RESP_48 |
                SDHCI_CMD_CRC_CHECK | SDHCI_CMD_INDEX_CHECK;

    sdhci_send_cmd(unit, cmd, arg, NULL);
}

/*
 * CMD53: SDIO multi-byte read (byte mode for small, block mode for large).
 */
BOOL sdio_cmd53_read(struct BrcmfUnit *unit, ULONG func, ULONG addr, UBYTE *buf, ULONG len)
{
    ULONG arg;
    UWORD cmd;
    ULONG i, words;
    BOOL block_mode = (len > 64);
    ULONG count;

    if (block_mode) {
        count = (len + unit->bn_BlockSize - 1) / unit->bn_BlockSize;
        arg = SDIO_CMD53_FUNC(func) | SDIO_CMD53_BLOCK_MODE |
              SDIO_CMD53_INCR_ADDR | SDIO_CMD53_ADDR(addr) | SDIO_CMD53_COUNT(count);
        sdhci_wr16(unit, SDHCI_BLOCK_SIZE, unit->bn_BlockSize);
        sdhci_wr16(unit, SDHCI_BLOCK_COUNT, count);
    } else {
        count = len;
        arg = SDIO_CMD53_FUNC(func) | SDIO_CMD53_INCR_ADDR |
              SDIO_CMD53_ADDR(addr) | SDIO_CMD53_COUNT(count);
        sdhci_wr16(unit, SDHCI_BLOCK_SIZE, len);
        sdhci_wr16(unit, SDHCI_BLOCK_COUNT, 1);
    }

    /* Transfer mode: read, single/multi block */
    sdhci_wr16(unit, SDHCI_TRANSFER_MODE,
               (1 << 4) |  /* Data direction: read */
               (block_mode ? (1 << 5) | (1 << 1) : 0)); /* Multi-block, block count enable */

    cmd = (SDIO_CMD_RW_EXTENDED << 8) | SDHCI_CMD_RESP_48 |
          SDHCI_CMD_CRC_CHECK | SDHCI_CMD_INDEX_CHECK | SDHCI_CMD_DATA;

    if (!sdhci_send_cmd(unit, cmd, arg, NULL))
        return FALSE;

    /* Read data from buffer */
    words = (len + 3) / 4;
    for (i = 0; i < words; i++) {
        /* Wait for buffer read ready */
        int tries = 10000;
        while (!(sdhci_rd(unit, SDHCI_PRESENT_STATE) & SDHCI_BUFFER_READ_EN) && --tries)
            ;
        if (!tries)
            return FALSE;

        ULONG val = sdhci_rd(unit, SDHCI_BUFFER_DATA);
        ULONG remaining = len - i * 4;
        if (remaining >= 4) {
            *(ULONG *)(buf + i * 4) = val;
        } else {
            UBYTE *p = (UBYTE *)&val;
            ULONG j;
            for (j = 0; j < remaining; j++)
                buf[i * 4 + j] = p[j];
        }
    }

    return sdhci_wait_data(unit);
}

/*
 * CMD53: SDIO multi-byte write.
 */
BOOL sdio_cmd53_write(struct BrcmfUnit *unit, ULONG func, ULONG addr, UBYTE *buf, ULONG len)
{
    ULONG arg;
    UWORD cmd;
    ULONG i, words;
    BOOL block_mode = (len > 64);
    ULONG count;

    if (block_mode) {
        count = (len + unit->bn_BlockSize - 1) / unit->bn_BlockSize;
        arg = SDIO_CMD53_WRITE | SDIO_CMD53_FUNC(func) | SDIO_CMD53_BLOCK_MODE |
              SDIO_CMD53_INCR_ADDR | SDIO_CMD53_ADDR(addr) | SDIO_CMD53_COUNT(count);
        sdhci_wr16(unit, SDHCI_BLOCK_SIZE, unit->bn_BlockSize);
        sdhci_wr16(unit, SDHCI_BLOCK_COUNT, count);
    } else {
        count = len;
        arg = SDIO_CMD53_WRITE | SDIO_CMD53_FUNC(func) | SDIO_CMD53_INCR_ADDR |
              SDIO_CMD53_ADDR(addr) | SDIO_CMD53_COUNT(count);
        sdhci_wr16(unit, SDHCI_BLOCK_SIZE, len);
        sdhci_wr16(unit, SDHCI_BLOCK_COUNT, 1);
    }

    /* Transfer mode: write */
    sdhci_wr16(unit, SDHCI_TRANSFER_MODE,
               (block_mode ? (1 << 5) | (1 << 1) : 0));

    cmd = (SDIO_CMD_RW_EXTENDED << 8) | SDHCI_CMD_RESP_48 |
          SDHCI_CMD_CRC_CHECK | SDHCI_CMD_INDEX_CHECK | SDHCI_CMD_DATA;

    if (!sdhci_send_cmd(unit, cmd, arg, NULL))
        return FALSE;

    /* Write data to buffer */
    words = (len + 3) / 4;
    for (i = 0; i < words; i++) {
        int tries = 10000;
        while (!(sdhci_rd(unit, SDHCI_PRESENT_STATE) & SDHCI_BUFFER_WRITE_EN) && --tries)
            ;
        if (!tries)
            return FALSE;

        ULONG val = 0;
        ULONG remaining = len - i * 4;
        if (remaining >= 4) {
            val = *(ULONG *)(buf + i * 4);
        } else {
            UBYTE *p = (UBYTE *)&val;
            ULONG j;
            for (j = 0; j < remaining; j++)
                p[j] = buf[i * 4 + j];
        }
        sdhci_wr(unit, SDHCI_BUFFER_DATA, val);
    }

    return sdhci_wait_data(unit);
}

/*
 * Initialize the SDHCI controller and enumerate the SDIO card.
 */
BOOL sdio_init(struct BrcmfUnit *unit)
{
    ULONG resp;
    UWORD cmd;
    UBYTE rev;

    D(bug("[brcmfmac] SDIO init at 0x%p\n", unit->bn_SdhciBase));

    /* Software reset */
    sdhci_wr8(unit, SDHCI_SOFTWARE_RST, 0x07);
    udelay(10000);

    /* Wait for reset complete */
    {
        int tries = 1000;
        while ((*(volatile UBYTE *)(unit->bn_SdhciBase + SDHCI_SOFTWARE_RST)) && --tries)
            udelay(100);
        if (!tries) {
            D(bug("[brcmfmac] SDHCI reset timeout\n"));
            return FALSE;
        }
    }

    /* Enable all interrupts */
    sdhci_wr(unit, SDHCI_INT_ENABLE, 0xFFFFFFFF);
    sdhci_wr(unit, SDHCI_SIGNAL_ENABLE, 0);

    /* Set clock to 400kHz for init */
    sdhci_wr16(unit, SDHCI_CLOCK_CTRL, 0);
    sdhci_wr16(unit, SDHCI_CLOCK_CTRL, (0x80 << 8) | 0x01); /* Divider=128, internal clock enable */
    udelay(10000);
    sdhci_wr16(unit, SDHCI_CLOCK_CTRL, (0x80 << 8) | 0x05); /* + SD clock enable */
    udelay(10000);

    /* Set power (3.3V) */
    sdhci_wr8(unit, SDHCI_POWER_CTRL, 0x0F); /* 3.3V, power on */
    udelay(10000);

    /* CMD5: IO_SEND_OP_COND (SDIO-specific) */
    cmd = (SD_CMD_IO_SEND_OP_COND << 8) | SDHCI_CMD_RESP_48;
    if (!sdhci_send_cmd(unit, cmd, 0, &resp)) {
        D(bug("[brcmfmac] CMD5 failed — no SDIO card\n"));
        return FALSE;
    }

    D(bug("[brcmfmac] CMD5 response: 0x%08lx\n", resp));

    /* CMD5 again with voltage window */
    if (!sdhci_send_cmd(unit, cmd, resp & 0x00FFFFFF, &resp)) {
        D(bug("[brcmfmac] CMD5 (with voltage) failed\n"));
        return FALSE;
    }

    /* CMD3: Get relative card address */
    cmd = (3 << 8) | SDHCI_CMD_RESP_48 | SDHCI_CMD_CRC_CHECK | SDHCI_CMD_INDEX_CHECK;
    if (!sdhci_send_cmd(unit, cmd, 0, &resp)) {
        D(bug("[brcmfmac] CMD3 failed\n"));
        return FALSE;
    }
    unit->bn_RCA = (resp >> 16) & 0xFFFF;
    D(bug("[brcmfmac] RCA: 0x%04x\n", unit->bn_RCA));

    /* CMD7: Select card */
    cmd = (SD_CMD_SELECT_CARD << 8) | SDHCI_CMD_RESP_48_BUSY |
          SDHCI_CMD_CRC_CHECK | SDHCI_CMD_INDEX_CHECK;
    if (!sdhci_send_cmd(unit, cmd, (ULONG)unit->bn_RCA << 16, &resp)) {
        D(bug("[brcmfmac] CMD7 failed\n"));
        return FALSE;
    }

    /* Increase clock to 25MHz */
    sdhci_wr16(unit, SDHCI_CLOCK_CTRL, 0);
    sdhci_wr16(unit, SDHCI_CLOCK_CTRL, (0x04 << 8) | 0x01); /* Divider=4 */
    udelay(10000);
    sdhci_wr16(unit, SDHCI_CLOCK_CTRL, (0x04 << 8) | 0x05);
    udelay(1000);

    /* Enable 4-bit bus width via CCCR */
    sdio_cmd52_write(unit, 0, SDIO_CCCR_BUS_IFACE, 0x02); /* 4-bit */

    /* Set host to 4-bit mode */
    {
        UBYTE ctrl = *(volatile UBYTE *)(unit->bn_SdhciBase + SDHCI_HOST_CTRL);
        ctrl |= 0x02; /* 4-bit data width */
        *(volatile UBYTE *)(unit->bn_SdhciBase + SDHCI_HOST_CTRL) = ctrl;
    }

    /* Set block size for function 1 and 2 */
    unit->bn_BlockSize = BRCMF_BLOCK_SIZE;
    sdio_cmd52_write(unit, 0, SDIO_CCCR_BLOCK_SIZE_1, BRCMF_BLOCK_SIZE & 0xFF);
    sdio_cmd52_write(unit, 0, SDIO_CCCR_BLOCK_SIZE_1 + 1, (BRCMF_BLOCK_SIZE >> 8) & 0xFF);
    sdio_cmd52_write(unit, 0, SDIO_CCCR_BLOCK_SIZE_2, BRCMF_BLOCK_SIZE & 0xFF);
    sdio_cmd52_write(unit, 0, SDIO_CCCR_BLOCK_SIZE_2 + 1, (BRCMF_BLOCK_SIZE >> 8) & 0xFF);

    /* Enable functions 1 and 2 */
    sdio_cmd52_write(unit, 0, SDIO_CCCR_IO_ENABLE, 0x06); /* Func 1 + Func 2 */

    /* Wait for functions ready */
    {
        int tries = 100;
        while (--tries) {
            UBYTE ready = sdio_cmd52_read(unit, 0, SDIO_CCCR_IO_READY);
            if ((ready & 0x06) == 0x06)
                break;
            udelay(10000);
        }
        if (!tries) {
            D(bug("[brcmfmac] SDIO functions not ready\n"));
            return FALSE;
        }
    }

    /* Read CCCR revision to verify communication */
    rev = sdio_cmd52_read(unit, 0, SDIO_CCCR_REVISION);
    D(bug("[brcmfmac] SDIO CCCR revision: 0x%02x\n", rev));

    /* Enable interrupts for function 1 and 2 */
    sdio_cmd52_write(unit, 0, SDIO_CCCR_INT_ENABLE, 0x07); /* Master + F1 + F2 */

    D(bug("[brcmfmac] SDIO init complete\n"));
    return TRUE;
}

/*
 * Backplane window management.
 * The BCM43455 backplane is accessed through a 32KB window.
 */
void brcmf_backplane_set_window(struct BrcmfUnit *unit, ULONG addr)
{
    ULONG window = addr & ~BRCMF_SDIO_SB_OFT_ADDR_MASK;

    if (window != unit->bn_BackplaneAddr) {
        sdio_cmd52_write(unit, BRCMF_SDIO_FUNC_BACKPLANE,
                         BRCMF_SDIO_BACKPLANE_ADDR_LOW, (window >> 8) & 0xFF);
        sdio_cmd52_write(unit, BRCMF_SDIO_FUNC_BACKPLANE,
                         BRCMF_SDIO_BACKPLANE_ADDR_MID, (window >> 16) & 0xFF);
        sdio_cmd52_write(unit, BRCMF_SDIO_FUNC_BACKPLANE,
                         BRCMF_SDIO_BACKPLANE_ADDR_HIGH, (window >> 24) & 0xFF);
        unit->bn_BackplaneAddr = window;
    }
}

ULONG brcmf_backplane_read32(struct BrcmfUnit *unit, ULONG addr)
{
    ULONG val = 0;
    brcmf_backplane_set_window(unit, addr);
    sdio_cmd53_read(unit, BRCMF_SDIO_FUNC_BACKPLANE,
                    (addr & BRCMF_SDIO_SB_OFT_ADDR_MASK) | BRCMF_SDIO_SB_ACCESS_2_4B_FLAG,
                    (UBYTE *)&val, 4);
    return val;
}

void brcmf_backplane_write32(struct BrcmfUnit *unit, ULONG addr, ULONG val)
{
    brcmf_backplane_set_window(unit, addr);
    sdio_cmd53_write(unit, BRCMF_SDIO_FUNC_BACKPLANE,
                     (addr & BRCMF_SDIO_SB_OFT_ADDR_MASK) | BRCMF_SDIO_SB_ACCESS_2_4B_FLAG,
                     (UBYTE *)&val, 4);
}
