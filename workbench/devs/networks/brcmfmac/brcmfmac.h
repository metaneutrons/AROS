#ifndef BRCMFMAC_H
 * Author: Fabian Schmieder
#define BRCMFMAC_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/semaphores.h>
#include <exec/devices.h>
#include <devices/sana2.h>
#include <devices/sana2wireless.h>

/*
 * BCM43455 WiFi driver for Raspberry Pi 4
 *
 * Architecture:
 * - SDIO transport via Arasan SDHCI controller (peribase + 0x300000)
 * - brcmfmac FullMAC firmware (embedded as const array)
 * - SANA-II wireless device interface
 */

#define BRCMF_DEVICE_NAME   "brcmfmac.device"
#define BRCMF_TASK_NAME     "BCM43455 WiFi"

/* Arasan SDHCI controller for WiFi (not EMMC2 which is for SD card) */
#define SDHCI_BASE_OFFSET   0x300000

/* SDHCI register offsets (SD Host Controller Specification) */
#define SDHCI_SDMA_ADDR     0x00
#define SDHCI_BLOCK_SIZE    0x04
#define SDHCI_BLOCK_COUNT   0x06
#define SDHCI_ARGUMENT      0x08
#define SDHCI_TRANSFER_MODE 0x0C
#define SDHCI_COMMAND       0x0E
#define SDHCI_RESPONSE0     0x10
#define SDHCI_RESPONSE1     0x14
#define SDHCI_RESPONSE2     0x18
#define SDHCI_RESPONSE3     0x1C
#define SDHCI_BUFFER_DATA   0x20
#define SDHCI_PRESENT_STATE 0x24
#define SDHCI_HOST_CTRL     0x28
#define SDHCI_POWER_CTRL    0x29
#define SDHCI_CLOCK_CTRL    0x2C
#define SDHCI_TIMEOUT_CTRL  0x2E
#define SDHCI_SOFTWARE_RST  0x2F
#define SDHCI_INT_STATUS    0x30
#define SDHCI_INT_ENABLE    0x34
#define SDHCI_SIGNAL_ENABLE 0x38

/* SDHCI command flags */
#define SDHCI_CMD_RESP_NONE     0x00
#define SDHCI_CMD_RESP_48       0x02
#define SDHCI_CMD_RESP_136      0x01
#define SDHCI_CMD_RESP_48_BUSY  0x03
#define SDHCI_CMD_CRC_CHECK     0x08
#define SDHCI_CMD_INDEX_CHECK   0x10
#define SDHCI_CMD_DATA          0x20

/* SDHCI present state bits */
#define SDHCI_CMD_INHIBIT       (1 << 0)
#define SDHCI_DAT_INHIBIT       (1 << 1)
#define SDHCI_BUFFER_READ_EN    (1 << 11)
#define SDHCI_BUFFER_WRITE_EN   (1 << 10)

/* SDHCI interrupt bits */
#define SDHCI_INT_CMD_COMPLETE  (1 << 0)
#define SDHCI_INT_XFER_COMPLETE (1 << 1)
#define SDHCI_INT_ERROR         (1 << 15)

/* SDIO commands */
#define SD_CMD_GO_IDLE          0
#define SD_CMD_IO_SEND_OP_COND  5
#define SD_CMD_SELECT_CARD      7
#define SDIO_CMD_RW_DIRECT      52  /* CMD52: single byte R/W */
#define SDIO_CMD_RW_EXTENDED    53  /* CMD53: multi-byte R/W */

/* SDIO CMD52 argument bits */
#define SDIO_CMD52_WRITE        (1 << 31)
#define SDIO_CMD52_FUNC(f)      (((f) & 7) << 28)
#define SDIO_CMD52_RAW          (1 << 27)
#define SDIO_CMD52_ADDR(a)      (((a) & 0x1FFFF) << 9)
#define SDIO_CMD52_DATA(d)      ((d) & 0xFF)

/* SDIO CMD53 argument bits */
#define SDIO_CMD53_WRITE        (1 << 31)
#define SDIO_CMD53_FUNC(f)      (((f) & 7) << 28)
#define SDIO_CMD53_BLOCK_MODE   (1 << 27)
#define SDIO_CMD53_INCR_ADDR    (1 << 26)
#define SDIO_CMD53_ADDR(a)      (((a) & 0x1FFFF) << 9)
#define SDIO_CMD53_COUNT(c)     ((c) & 0x1FF)

/* SDIO CCCR (Common Card Control Registers) */
#define SDIO_CCCR_REVISION      0x00
#define SDIO_CCCR_IO_ENABLE     0x02
#define SDIO_CCCR_IO_READY      0x03
#define SDIO_CCCR_INT_ENABLE    0x04
#define SDIO_CCCR_BUS_IFACE     0x07
#define SDIO_CCCR_BLOCK_SIZE_0  0x10  /* Function 0 block size */
#define SDIO_CCCR_BLOCK_SIZE_1  0x110 /* Function 1 block size */
#define SDIO_CCCR_BLOCK_SIZE_2  0x210 /* Function 2 block size */

/* brcmfmac SDIO function assignments */
#define BRCMF_SDIO_FUNC_BACKPLANE  1  /* Chip backplane access */
#define BRCMF_SDIO_FUNC_FRAME      2  /* Data frames */

/* Backplane window register */
#define BRCMF_SDIO_SB_OFT_ADDR_MASK    0x7FFF
#define BRCMF_SDIO_SB_ACCESS_2_4B_FLAG 0x08000
#define BRCMF_SDIO_BACKPLANE_ADDR_LOW   0x1000A
#define BRCMF_SDIO_BACKPLANE_ADDR_MID   0x1000B
#define BRCMF_SDIO_BACKPLANE_ADDR_HIGH  0x1000C

/* Chip constants for BCM43455 (CYW43455) */
#define BCM43455_CHIP_ID        0x4345
#define BCM43455_RAMBASE        0x198000
#define BCM43455_RAMSIZE        0xC8000   /* 800KB */
#define BCM43455_SR_MEMSIZE     0x40000   /* 256KB */

/* Frame header for brcmfmac SDIO data frames */
struct BrcmfSdioFrameHdr {
    UWORD len;          /* Total frame length including header */
    UWORD notlen;       /* ~len for validation */
    UBYTE seq;          /* Sequence number */
    UBYTE chan;          /* Channel: 0=control, 1=event, 2=data */
    UBYTE nextlen;      /* Next frame length (0 if last) */
    UBYTE dataoff;      /* Offset to data from start of header */
    UBYTE flow;         /* Flow control */
    UBYTE credit;       /* TX credit */
    UBYTE reserved[2];
};

/* Channel IDs */
#define BRCMF_CHAN_CONTROL  0
#define BRCMF_CHAN_EVENT    1
#define BRCMF_CHAN_DATA     2

/* Max frame size */
#define BRCMF_MAX_FRAME     1600
#define BRCMF_BLOCK_SIZE    64

/* Network constants */
#define ETH_ADDRSIZE        6
#define ETH_MTU             1500
#define ETH_HEADERSIZE      14
#define ETH_MAXPACKETSIZE   (ETH_MTU + ETH_HEADERSIZE + 4)

#define MAX_SCAN_RESULTS    32
#define SSID_MAX_LEN        32

/* Scan result entry */
struct BrcmfScanResult {
    UBYTE bssid[ETH_ADDRSIZE];
    UBYTE ssid[SSID_MAX_LEN + 1];
    UBYTE ssid_len;
    WORD  rssi;
    UWORD channel;
    UWORD auth_type;    /* S2INFO encryption flags */
};

/* Driver unit (one per WiFi chip) */
struct BrcmfUnit {
    struct Node         bn_Node;
    ULONG               bn_UnitNum;
    ULONG               bn_Flags;

    struct BrcmfBase    *bn_Device;
    IPTR                bn_SdhciBase;   /* SDHCI register base */

    /* MAC address */
    UBYTE               bn_DevAddr[ETH_ADDRSIZE];

    /* SDIO state */
    UWORD               bn_RCA;         /* Relative Card Address */
    ULONG               bn_BlockSize;

    /* Backplane window */
    ULONG               bn_BackplaneAddr;

    /* Network state */
    BOOL                bn_Associated;
    UBYTE               bn_SSID[SSID_MAX_LEN + 1];
    UBYTE               bn_BSSID[ETH_ADDRSIZE];
    WORD                bn_RSSI;

    /* Scan results */
    struct BrcmfScanResult bn_ScanResults[MAX_SCAN_RESULTS];
    ULONG               bn_ScanCount;
    BOOL                bn_Scanning;

    /* Task and signals */
    struct Task         *bn_Task;
    ULONG               bn_IntSig;
    APTR                bn_IRQHandle;

    /* SANA-II request queues */
    struct MinList      bn_ReadList;
    struct MinList      bn_WriteList;
    struct MinList      bn_EventList;

    struct SignalSemaphore bn_Lock;

    /* Statistics */
    struct Sana2DeviceStats bn_Stats;

    /* TX sequence number */
    UBYTE               bn_TxSeq;
};

/* Unit flags */
#define BNF_ONLINE      (1 << 0)
#define BNF_CONFIGURED  (1 << 1)

/* Driver base */
struct BrcmfBase {
    struct Device       bn_Device;
    struct BrcmfUnit    *bn_Units[1];
    ULONG               bn_UnitCount;
    struct SignalSemaphore bn_Lock;
};

/* SDIO transport functions */
BOOL sdio_init(struct BrcmfUnit *unit);
UBYTE sdio_cmd52_read(struct BrcmfUnit *unit, ULONG func, ULONG addr);
void sdio_cmd52_write(struct BrcmfUnit *unit, ULONG func, ULONG addr, UBYTE val);
BOOL sdio_cmd53_read(struct BrcmfUnit *unit, ULONG func, ULONG addr, UBYTE *buf, ULONG len);
BOOL sdio_cmd53_write(struct BrcmfUnit *unit, ULONG func, ULONG addr, UBYTE *buf, ULONG len);

/* Backplane access */
void brcmf_backplane_set_window(struct BrcmfUnit *unit, ULONG addr);
ULONG brcmf_backplane_read32(struct BrcmfUnit *unit, ULONG addr);
void brcmf_backplane_write32(struct BrcmfUnit *unit, ULONG addr, ULONG val);

/* Firmware upload */
BOOL brcmf_firmware_upload(struct BrcmfUnit *unit);

/* FullMAC commands */
BOOL brcmf_cmd_scan(struct BrcmfUnit *unit);
BOOL brcmf_cmd_join(struct BrcmfUnit *unit, const UBYTE *ssid, ULONG ssid_len,
                    const UBYTE *key, ULONG key_len, ULONG auth_type);
BOOL brcmf_cmd_disassoc(struct BrcmfUnit *unit);
BOOL brcmf_cmd_get_mac(struct BrcmfUnit *unit);

/* Frame TX/RX */
BOOL brcmf_send_frame(struct BrcmfUnit *unit, UBYTE *data, ULONG len);
LONG brcmf_recv_frame(struct BrcmfUnit *unit, UBYTE *buf, ULONG maxlen);

/* Unit task */
void brcmf_UnitTask(void);

#endif /* BRCMFMAC_H */
