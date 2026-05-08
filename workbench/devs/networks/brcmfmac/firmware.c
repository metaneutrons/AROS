/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Author: Fabian Schmieder
 */
/*
 * BCM43455 WiFi — Firmware upload
 *
 * Uploads the embedded brcmfmac43455-sdio firmware to the chip's
 * RAM via the SDIO backplane interface.
 */

#include <exec/types.h>
#include <aros/debug.h>
#include <string.h>

#include "brcmfmac.h"

/* Firmware data (generated from linux-firmware brcm/brcmfmac43455-sdio.*) */
extern const UBYTE brcmf_firmware_bin[];
extern const ULONG brcmf_firmware_bin_size;
extern const UBYTE brcmf_nvram[];
extern const ULONG brcmf_nvram_size;
extern const UBYTE brcmf_clm_blob[];
extern const ULONG brcmf_clm_blob_size;

/* ARM CR4 core registers (for halting/releasing the WiFi CPU) */
#define ARMCR4_BANKIDX      0x40
#define ARMCR4_BANKPDA      0x4C
#define ARMCR4_CAP          0x04
#define ARMCR4_IOCTL        0x408
#define ARMCR4_IOST         0x500
#define ARMCR4_RESETCTL     0x800

#define ARMCR4_CPUHALT      0x0020

/* Chip common core base for BCM43455 */
#define CHIPCOMMON_BASE     0x18000000
#define CHIPCOMMON_SR_CONTROL 0x504

/* SOCRAM wrapper base */
#define SOCRAM_WRAP_BASE    0x18104000

/*
 * Upload firmware binary to chip RAM.
 *
 * Sequence:
 * 1. Halt the ARM core
 * 2. Write firmware to RAM starting at BCM43455_RAMBASE
 * 3. Write NVRAM at end of RAM (top - nvram_padded_size)
 * 4. Write NVRAM size token at very last word of RAM
 * 5. Release ARM core
 */
BOOL brcmf_firmware_upload(struct BrcmfUnit *unit)
{
    ULONG i, offset;
    ULONG nvram_pad_size;
    ULONG nvram_token;

    D(bug("[brcmfmac] Firmware upload: bin=%ld bytes, nvram=%ld bytes\n",
          brcmf_firmware_bin_size, brcmf_nvram_size));

    if (brcmf_firmware_bin_size == 0) {
        D(bug("[brcmfmac] ERROR: No firmware embedded\n"));
        return FALSE;
    }

    /* Step 1: Halt ARM core via reset control */
    brcmf_backplane_write32(unit, SOCRAM_WRAP_BASE + ARMCR4_RESETCTL, ARMCR4_CPUHALT);

    /* Step 2: Upload firmware to chip RAM */
    D(bug("[brcmfmac] Writing firmware to 0x%08lx...\n", BCM43455_RAMBASE));

    for (offset = 0; offset < brcmf_firmware_bin_size; offset += 64) {
        ULONG chunk = brcmf_firmware_bin_size - offset;
        if (chunk > 64)
            chunk = 64;

        brcmf_backplane_set_window(unit, BCM43455_RAMBASE + offset);
        sdio_cmd53_write(unit, BRCMF_SDIO_FUNC_BACKPLANE,
                         ((BCM43455_RAMBASE + offset) & BRCMF_SDIO_SB_OFT_ADDR_MASK) |
                         BRCMF_SDIO_SB_ACCESS_2_4B_FLAG,
                         (UBYTE *)&brcmf_firmware_bin[offset], chunk);
    }

    /* Step 3: Write NVRAM at end of RAM */
    nvram_pad_size = (brcmf_nvram_size + 3) & ~3; /* Pad to 4 bytes */
    offset = BCM43455_RAMBASE + BCM43455_RAMSIZE - 4 - nvram_pad_size;

    D(bug("[brcmfmac] Writing NVRAM to 0x%08lx (%ld bytes)\n", offset, nvram_pad_size));

    for (i = 0; i < nvram_pad_size; i += 64) {
        ULONG chunk = nvram_pad_size - i;
        if (chunk > 64)
            chunk = 64;

        brcmf_backplane_set_window(unit, offset + i);
        sdio_cmd53_write(unit, BRCMF_SDIO_FUNC_BACKPLANE,
                         ((offset + i) & BRCMF_SDIO_SB_OFT_ADDR_MASK) |
                         BRCMF_SDIO_SB_ACCESS_2_4B_FLAG,
                         (UBYTE *)&brcmf_nvram[i],
                         (i + chunk <= brcmf_nvram_size) ? chunk : brcmf_nvram_size - i);
    }

    /* Step 4: Write NVRAM size token (complement of size in words) */
    nvram_token = (~(nvram_pad_size / 4) << 16) | (nvram_pad_size / 4);
    offset = BCM43455_RAMBASE + BCM43455_RAMSIZE - 4;
    brcmf_backplane_write32(unit, offset, nvram_token);

    /* Step 5: Release ARM core */
    D(bug("[brcmfmac] Releasing ARM core...\n"));
    brcmf_backplane_write32(unit, SOCRAM_WRAP_BASE + ARMCR4_RESETCTL, 0);

    /* Wait for firmware to boot (~100ms) */
    {
        volatile ULONG d;
        for (d = 0; d < 1000000; d++) ;
    }

    D(bug("[brcmfmac] Firmware upload complete\n"));

    return TRUE;
}
