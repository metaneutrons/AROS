/*
    Copyright (C) 2013, The AROS Development Team. All rights reserved.
*/

#define DEBUG 0
#include <aros/debug.h>

#include <hardware/mmc.h>
#include <hardware/sdhc.h>

#include "sdcard_base.h"
#include "sdcard_unit.h"

ULONG FNAME_SDCUNIT(MMCSwitch)(UBYTE offset, UBYTE value, struct sdcard_Unit *sdcUnit)
{
    struct TagItem sdcSwitchTags[] =
    {
        {SDCARD_TAG_CMD,        MMC_CMD_SWITCH},
        {SDCARD_TAG_ARG,        0},
        {SDCARD_TAG_RSPTYPE,    MMC_RSP_R1b},
        {SDCARD_TAG_RSP,        0},
        {TAG_DONE,              0}
    };
    ULONG retVal;

    D(bug("[SDCard%02ld] %s()\n", sdcUnit->sdcu_UnitNum, __PRETTY_FUNCTION__));

    sdcSwitchTags[1].ti_Data = (MMC_SWITCH_WRITE_BYTE << 24) | (offset << 16) | (value << 8);

    if ((retVal = FNAME_SDCBUS(SendCmd)(sdcSwitchTags, sdcUnit->sdcu_Bus)) != -1)
    {
        if ((retVal = FNAME_SDCBUS(WaitCmd)(SDHCI_PS_CMD_INHIBIT|SDHCI_PS_DATA_INHIBIT, 1000, sdcUnit->sdcu_Bus)) != -1)
        {
            /* Wait for units ready status */
            FNAME_SDCUNIT(WaitStatus)(1000, sdcUnit);
        }
    }
    return retVal;
}

ULONG FNAME_SDCUNIT(MMCChangeFrequency)(struct sdcard_Unit *sdcUnit)
{
    UBYTE               sdcRespBuf[512];
    UBYTE               sdcCardType;
    struct TagItem      sdcChFreqTags[] =
    {
        {SDCARD_TAG_CMD,        MMC_CMD_SEND_EXT_CSD},
        {SDCARD_TAG_ARG,        0},
        {SDCARD_TAG_RSPTYPE,    MMC_RSP_R1},
        {SDCARD_TAG_RSP,        0},
        {SDCARD_TAG_DATA,       (IPTR)sdcRespBuf},
        {SDCARD_TAG_DATALEN,    512},
        {SDCARD_TAG_DATAFLAGS,  MMC_DATA_READ},
        {TAG_DONE,              0}
    };

    D(bug("[SDCard%02ld] %s()\n", sdcUnit->sdcu_UnitNum, __PRETTY_FUNCTION__));

    if (sdcUnit->sdcu_Bus->sdcb_BusFlags & AF_Bus_SPI)
        return 0;

    D(bug("[SDCard%02ld] %s: Querying Ext_CSD ... \n", sdcUnit->sdcu_UnitNum, __PRETTY_FUNCTION__));
    if ((FNAME_SDCBUS(SendCmd)(sdcChFreqTags, sdcUnit->sdcu_Bus) == -1) || (FNAME_SDCBUS(WaitCmd)(SDHCI_PS_CMD_INHIBIT|SDHCI_PS_DATA_INHIBIT, 1000, sdcUnit->sdcu_Bus) == -1))
    {
        D(bug("[SDCard%02ld] %s: Query Failed\n", sdcUnit->sdcu_UnitNum, __PRETTY_FUNCTION__));
        return 0;
    }
    D(bug("[SDCard%02ld] %s: Query Response = %08x\n", sdcUnit->sdcu_UnitNum, __PRETTY_FUNCTION__, sdcChFreqTags[3].ti_Data));

    sdcCardType = sdcRespBuf[0xC4] & 0xF;

    D(bug("[SDCard%02ld] %s: CardType = %d\n", sdcUnit->sdcu_UnitNum, __PRETTY_FUNCTION__, sdcCardType));

    /*
     * Try HS200 first (200MHz, 1.8V) if card supports it.
     * HS200 requires: card supports it (EXT_CSD[196] bit 4) AND
     * no card-detect pin (eMMC is soldered, no CD).
     * On BCM2712 (CM5) eMMC always runs at 1.8V I/O — no voltage switch needed.
     */
    if ((sdcCardType & MMC_HS200_1V8) &&
        !(sdcUnit->sdcu_Bus->sdcb_IOReadLong(SDHCI_PRESENT_STATE, sdcUnit->sdcu_Bus) & SDHCI_PS_CARD_DETECT_PIN_LVL))
    {
        D(bug("[SDCard%02ld] %s: Card supports HS200, attempting switch\n",
              sdcUnit->sdcu_UnitNum, __PRETTY_FUNCTION__));

        /* Set bus width to 4-bit before HS200 switch (required by spec) */
        if (FNAME_SDCUNIT(MMCSwitch)(EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_4, sdcUnit) != -1)
        {
            UBYTE hostCtrl = sdcUnit->sdcu_Bus->sdcb_IOReadByte(SDHCI_HOST_CONTROL, sdcUnit->sdcu_Bus);
            hostCtrl |= SDHCI_HCTRL_4BITBUS;
            sdcUnit->sdcu_Bus->sdcb_IOWriteByte(SDHCI_HOST_CONTROL, hostCtrl, sdcUnit->sdcu_Bus);
            sdcUnit->sdcu_Flags |= AF_Card_4bitData;
        }

        /* Switch to HS200 timing */
        if (FNAME_SDCUNIT(MMCSwitch)(EXT_CSD_HS_TIMING, EXT_CSD_HS_TIMING_HS200, sdcUnit) != -1)
        {
            /* Set UHS mode in HOST_CONTROL2 */
            UWORD ctrl2 = sdcUnit->sdcu_Bus->sdcb_IOReadWord(SDHCI_HOST_CONTROL2, sdcUnit->sdcu_Bus);
            ctrl2 &= ~SDHCI_CTRL2_UHS_MASK;
            ctrl2 |= SDHCI_CTRL2_HS200;
            sdcUnit->sdcu_Bus->sdcb_IOWriteWord(SDHCI_HOST_CONTROL2, ctrl2, sdcUnit->sdcu_Bus);

            sdcUnit->sdcu_Flags |= AF_Card_HighSpeed | AF_Card_HS200;

            D(bug("[SDCard%02ld] %s: HS200 mode enabled\n",
                  sdcUnit->sdcu_UnitNum, __PRETTY_FUNCTION__));
            return 0;
        }
        D(bug("[SDCard%02ld] %s: HS200 switch failed, falling back to HS\n",
              sdcUnit->sdcu_UnitNum, __PRETTY_FUNCTION__));
    }

    /* Fallback: standard High-Speed (26/52 MHz) */
    if (FNAME_SDCUNIT(MMCSwitch)(EXT_CSD_HS_TIMING, EXT_CSD_HS_TIMING_HS, sdcUnit) != -1)
    {
        D(bug("[SDCard%02ld] %s: HS switch failed\n", sdcUnit->sdcu_UnitNum, __PRETTY_FUNCTION__));
        return -1;
    }

    if ((FNAME_SDCBUS(SendCmd)(sdcChFreqTags, sdcUnit->sdcu_Bus) != -1) && (FNAME_SDCBUS(WaitCmd)(SDHCI_PS_CMD_INHIBIT|SDHCI_PS_DATA_INHIBIT, 1000, sdcUnit->sdcu_Bus) != -1))
    {
        if (sdcRespBuf[0xB9])
        {
            /* MMC supports 26MHz High-Speed mode .. */
            sdcUnit->sdcu_Flags |= AF_Card_HighSpeed;

            /* is 52MHz mode also supported? */
            if (sdcCardType & MMC_HS_52MHZ)
                sdcUnit->sdcu_Flags |= AF_Card_HighSpeed52;
        }
    }
    return 0;
}
