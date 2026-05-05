/*
 * BCM43455 WiFi — Device initialization
 */

#include <aros/debug.h>
#include <aros/symbolsets.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/kernel.h>

#include "brcmfmac.h"

#include LC_LIBDEFS_FILE

#define PERIBASE 0xFE000000

static int brcmf_Init(LIBBASETYPEPTR LIBBASE)
{
    struct BrcmfUnit *unit;

    D(bug("[brcmfmac] Init\n"));

    InitSemaphore(&LIBBASE->bn_Lock);

    unit = AllocVec(sizeof(struct BrcmfUnit), MEMF_CLEAR | MEMF_PUBLIC);
    if (!unit)
        return FALSE;

    unit->bn_UnitNum = 0;
    unit->bn_Device = LIBBASE;
    unit->bn_SdhciBase = PERIBASE + SDHCI_BASE_OFFSET;
    unit->bn_BlockSize = BRCMF_BLOCK_SIZE;

    InitSemaphore(&unit->bn_Lock);
    NEWLIST(&unit->bn_ReadList);
    NEWLIST(&unit->bn_WriteList);
    NEWLIST(&unit->bn_EventList);

    /* Initialize SDIO and detect the WiFi chip */
    if (!sdio_init(unit)) {
        D(bug("[brcmfmac] SDIO init failed — no WiFi chip detected\n"));
        FreeVec(unit);
        return TRUE; /* Not fatal — system works without WiFi */
    }

    /* Upload firmware */
    if (!brcmf_firmware_upload(unit)) {
        D(bug("[brcmfmac] Firmware upload failed\n"));
        FreeVec(unit);
        return TRUE;
    }

    /* Get MAC address */
    brcmf_cmd_get_mac(unit);

    LIBBASE->bn_Units[0] = unit;
    LIBBASE->bn_UnitCount = 1;

    D(bug("[brcmfmac] BCM43455 WiFi initialized\n"));

    return TRUE;
}

static int brcmf_Expunge(LIBBASETYPEPTR LIBBASE)
{
    if (LIBBASE->bn_Units[0]) {
        FreeVec(LIBBASE->bn_Units[0]);
        LIBBASE->bn_Units[0] = NULL;
    }
    return TRUE;
}

ADD2INITLIB(brcmf_Init, 0)
ADD2EXPUNGELIB(brcmf_Expunge, 0)
