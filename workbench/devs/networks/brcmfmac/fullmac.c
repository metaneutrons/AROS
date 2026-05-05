/*
 * BCM43455 WiFi — FullMAC command interface
 *
 * Sends ioctl commands to the firmware and processes events/responses.
 * The BCM43455 runs a FullMAC firmware that handles 802.11 internally;
 * the host driver only sends high-level commands (scan, join, etc.)
 * and receives events (scan results, link up/down, RX frames).
 */

#include <exec/types.h>
#include <aros/debug.h>
#include <string.h>

#include "brcmfmac.h"

/* brcmfmac ioctl command IDs (from Linux brcmfmac/fwil_types.h) */
#define BRCMF_C_GET_SSID        25
#define BRCMF_C_SET_SSID        26
#define BRCMF_C_GET_CHANNEL     29
#define BRCMF_C_SET_CHANNEL     30
#define BRCMF_C_DISASSOC        52
#define BRCMF_C_GET_MACADDR     54
#define BRCMF_C_SCAN            50
#define BRCMF_C_SCAN_RESULTS    51
#define BRCMF_C_SET_PASSIVE_SCAN 49
#define BRCMF_C_SET_WSEC        134
#define BRCMF_C_SET_WPA_AUTH    165
#define BRCMF_C_SET_KEY         45
#define BRCMF_C_UP              2
#define BRCMF_C_DOWN            3
#define BRCMF_C_SET_INFRA       20
#define BRCMF_C_SET_AUTH        22

/*
 * Send an ioctl command to the firmware via SDIO control channel.
 */
static BOOL brcmf_send_ioctl(struct BrcmfUnit *unit, ULONG cmd,
                              UBYTE *data, ULONG len, BOOL set)
{
    /* Frame format: SDIO header + CDC header + data */
    UBYTE frame[BRCMF_MAX_FRAME];
    struct BrcmfSdioFrameHdr *hdr = (struct BrcmfSdioFrameHdr *)frame;
    ULONG total_len;
    ULONG *cdc;

    /* CDC (Common Device Control) header: cmd, len, flags, status */
    ULONG cdc_hdr_size = 16; /* 4 ULONGs */
    total_len = sizeof(struct BrcmfSdioFrameHdr) + cdc_hdr_size + len;

    if (total_len > BRCMF_MAX_FRAME)
        return FALSE;

    memset(frame, 0, total_len);

    /* SDIO frame header */
    hdr->len = total_len;
    hdr->notlen = ~total_len;
    hdr->seq = unit->bn_TxSeq++;
    hdr->chan = BRCMF_CHAN_CONTROL;
    hdr->dataoff = sizeof(struct BrcmfSdioFrameHdr) + cdc_hdr_size;

    /* CDC header */
    cdc = (ULONG *)(frame + sizeof(struct BrcmfSdioFrameHdr));
    cdc[0] = cmd;                           /* Command */
    cdc[1] = len;                           /* Output buffer length */
    cdc[2] = set ? 0x02 : 0x00;            /* Flags: SET=2, GET=0 */
    cdc[3] = 0;                             /* Status (filled by firmware) */

    /* Payload */
    if (data && len > 0)
        memcpy(frame + sizeof(struct BrcmfSdioFrameHdr) + cdc_hdr_size, data, len);

    return brcmf_send_frame(unit, frame, total_len);
}

/*
 * Get MAC address from firmware.
 */
BOOL brcmf_cmd_get_mac(struct BrcmfUnit *unit)
{
    UBYTE buf[ETH_ADDRSIZE];

    if (!brcmf_send_ioctl(unit, BRCMF_C_GET_MACADDR, buf, ETH_ADDRSIZE, FALSE))
        return FALSE;

    /* TODO: Read response from RX frame */
    /* For now, MAC will be read from OTP during firmware init */

    return TRUE;
}

/*
 * Initiate a WiFi scan.
 */
BOOL brcmf_cmd_scan(struct BrcmfUnit *unit)
{
    UBYTE scan_params[64];

    D(bug("[brcmfmac] Starting scan\n"));

    memset(scan_params, 0, sizeof(scan_params));

    /* Set passive scan first */
    {
        ULONG val = 0; /* 0 = active scan */
        brcmf_send_ioctl(unit, BRCMF_C_SET_PASSIVE_SCAN, (UBYTE *)&val, 4, TRUE);
    }

    unit->bn_Scanning = TRUE;
    unit->bn_ScanCount = 0;

    /* Trigger scan — firmware will send scan results as events */
    return brcmf_send_ioctl(unit, BRCMF_C_SCAN, scan_params, 64, TRUE);
}

/*
 * Join/associate with a network.
 */
BOOL brcmf_cmd_join(struct BrcmfUnit *unit, const UBYTE *ssid, ULONG ssid_len,
                    const UBYTE *key, ULONG key_len, ULONG auth_type)
{
    UBYTE join_params[128];

    D(bug("[brcmfmac] Joining SSID: %.*s\n", ssid_len, ssid));

    memset(join_params, 0, sizeof(join_params));

    /* Set infrastructure mode */
    {
        ULONG val = 1; /* Infrastructure */
        brcmf_send_ioctl(unit, BRCMF_C_SET_INFRA, (UBYTE *)&val, 4, TRUE);
    }

    /* Set authentication type */
    {
        ULONG val = (auth_type > 0) ? 1 : 0; /* Open or WPA */
        brcmf_send_ioctl(unit, BRCMF_C_SET_AUTH, (UBYTE *)&val, 4, TRUE);
    }

    /* Set SSID to trigger association */
    /* join_params format: ULONG ssid_len + UBYTE ssid[32] + ... */
    *(ULONG *)join_params = ssid_len;
    memcpy(join_params + 4, ssid, ssid_len);

    return brcmf_send_ioctl(unit, BRCMF_C_SET_SSID, join_params, 36, TRUE);
}

/*
 * Disassociate from current network.
 */
BOOL brcmf_cmd_disassoc(struct BrcmfUnit *unit)
{
    D(bug("[brcmfmac] Disassociating\n"));

    unit->bn_Associated = FALSE;
    return brcmf_send_ioctl(unit, BRCMF_C_DISASSOC, NULL, 0, TRUE);
}

/*
 * Send a data frame to the firmware for transmission.
 */
BOOL brcmf_send_frame(struct BrcmfUnit *unit, UBYTE *data, ULONG len)
{
    /* Pad to block size */
    ULONG padded = (len + unit->bn_BlockSize - 1) & ~(unit->bn_BlockSize - 1);

    return sdio_cmd53_write(unit, BRCMF_SDIO_FUNC_FRAME, 0, data, padded);
}

/*
 * Receive a frame from the firmware.
 * Returns frame length, or -1 if no frame available.
 */
LONG brcmf_recv_frame(struct BrcmfUnit *unit, UBYTE *buf, ULONG maxlen)
{
    struct BrcmfSdioFrameHdr *hdr;
    ULONG len;

    /* Read the frame header first */
    if (!sdio_cmd53_read(unit, BRCMF_SDIO_FUNC_FRAME, 0, buf,
                         sizeof(struct BrcmfSdioFrameHdr)))
        return -1;

    hdr = (struct BrcmfSdioFrameHdr *)buf;
    len = hdr->len;

    /* Validate */
    if (len == 0 || len > maxlen || (UWORD)(len ^ hdr->notlen) != 0xFFFF)
        return -1;

    /* Read the rest of the frame */
    if (len > sizeof(struct BrcmfSdioFrameHdr)) {
        ULONG remaining = len - sizeof(struct BrcmfSdioFrameHdr);
        if (!sdio_cmd53_read(unit, BRCMF_SDIO_FUNC_FRAME, 0,
                             buf + sizeof(struct BrcmfSdioFrameHdr), remaining))
            return -1;
    }

    return (LONG)len;
}
