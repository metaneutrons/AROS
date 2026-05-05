/*
 * BCM43455 WiFi — Firmware upload
 *
 * Uploads the embedded brcmfmac43455-sdio firmware blob to the chip's
 * RAM via the SDIO backplane interface.
 *
 * TODO: Embed actual firmware binary. For now this is a stub that
 * will be completed when firmware blob is integrated.
 */

#include <exec/types.h>
#include <aros/debug.h>

#include "brcmfmac.h"

/*
 * The firmware binary will be embedded here as:
 * static const UBYTE firmware_bin[] = { ... };
 * static const ULONG firmware_bin_size = sizeof(firmware_bin);
 *
 * For now, define empty placeholders.
 */
static const UBYTE firmware_bin[] = { 0 };
static const ULONG firmware_bin_size = 0;

static const UBYTE nvram_txt[] =
    "boardtype=0x0811\0"
    "boardrev=0x1101\0"
    "boardflags=0x00404001\0"
    "boardflags3=0x08000000\0"
    "macaddr=00:90:4c:c5:12:38\0"
    "ccode=ALL\0"
    "regrev=0\0"
    "\0";

BOOL brcmf_firmware_upload(struct BrcmfUnit *unit)
{
    D(bug("[brcmfmac] Firmware upload\n"));

    if (firmware_bin_size == 0) {
        D(bug("[brcmfmac] WARNING: No firmware embedded — WiFi will not function\n"));
        D(bug("[brcmfmac] Embed brcmfmac43455-sdio.bin to enable WiFi\n"));
        return FALSE;
    }

    /*
     * Upload sequence:
     * 1. Halt ARM core: write to backplane ARMCR4 reset vector
     * 2. Upload firmware to chip RAM at BCM43455_RAMBASE
     * 3. Upload NVRAM to end of RAM (top - nvram_size, padded to 4 bytes)
     * 4. Write NVRAM length token at very end of RAM
     * 5. Release ARM core reset
     */

    /* TODO: Implement when firmware blob is available */

    return TRUE;
}
