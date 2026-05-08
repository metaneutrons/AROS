/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Author: Fabian Schmieder
 */
/*
 * DesignWare MAC — Device initialization (for RP1 on RPi5)
 */

#include <aros/debug.h>
#include <aros/symbolsets.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/kernel.h>

#include "dwmac.h"

#include LC_LIBDEFS_FILE

/* RP1 Ethernet offset within BAR1 */
#define RP1_ETH_OFFSET  0x180000

APTR KernelBase = NULL;

static int dwmac_Init(LIBBASETYPEPTR LIBBASE)
{
    struct DWMACUnit *unit;
    IPTR peribase;

    D(bug("[dwmac] Init\n"));

    KernelBase = OpenResource("kernel.resource");
    if (!KernelBase)
        return FALSE;

    /*
     * The DesignWare MAC is only present on RPi5 (behind RP1).
     * On RPi4, peribase is 0xFE000000 and there's no DWMAC.
     * On RPi5, we need the RP1 BAR1 address from the RP1 resource.
     *
     * For now, detect RPi5 by checking if peribase != 0xFE000000.
     * The actual RP1 BAR1 address will be provided by rp1.resource.
     */
    peribase = KrnGetSystemAttr(KATTR_PeripheralBase);
    if (peribase == 0xFE000000) {
        D(bug("[dwmac] RPi4 detected — no DesignWare MAC\n"));
        return TRUE; /* Not an error, just not present */
    }

    D(bug("[dwmac] RPi5 detected — looking for RP1 Ethernet\n"));

    InitSemaphore(&LIBBASE->du_Lock);

    unit = AllocVec(sizeof(struct DWMACUnit), MEMF_CLEAR | MEMF_PUBLIC);
    if (!unit)
        return TRUE;

    unit->du_UnitNum = 0;
    unit->du_Device = LIBBASE;
    unit->du_PhyAddr = 1; /* Default PHY address */

    /*
     * TODO: Get RP1 BAR1 from rp1.resource.
     * For now, use a placeholder that will be filled by RP1 init.
     * The RP1 resource sets up the BAR1 mapping and exports it.
     */
    /* Get RP1 BAR1 from rp1.resource (initialized at pri 80, before us at -60) */
    {
        struct Library *rp1base = OpenResource("rp1.resource");
        if (rp1base) {
            /* RP1Base struct starts with Library, then BOOL + IPTR BAR1 */
            IPTR *fields = (IPTR *)((UBYTE *)rp1base + sizeof(struct Library));
            if (fields[0]) { /* rp1_Present */
                unit->du_RegBase = fields[1] + 0x180000; /* BAR1 + ETH offset */
                D(bug("[dwmac] Got RP1 ETH base: 0x%p\n", unit->du_RegBase));
            }
        }
        if (!unit->du_RegBase) {
            D(bug("[dwmac] RP1 not available\n"));
            FreeVec(unit);
            return TRUE;
        }
    }

    InitSemaphore(&unit->du_Lock);
    NEWLIST(&unit->du_ReadList);
    NEWLIST(&unit->du_WriteList);

    /* Allocate DMA descriptors (16-byte aligned) */
    unit->du_TxDesc = AllocVec(TX_RING_SIZE * sizeof(struct DWMACDesc) + 16,
                               MEMF_CLEAR | MEMF_PUBLIC | MEMF_31BIT);
    unit->du_RxDesc = AllocVec(RX_RING_SIZE * sizeof(struct DWMACDesc) + 16,
                               MEMF_CLEAR | MEMF_PUBLIC | MEMF_31BIT);

    /* Allocate TX/RX buffers */
    unit->du_TxBuf = AllocVec(TX_RING_SIZE * ETH_BUF_SIZE, MEMF_CLEAR | MEMF_PUBLIC);
    unit->du_RxBuf = AllocVec(RX_RING_SIZE * ETH_BUF_SIZE, MEMF_CLEAR | MEMF_PUBLIC);

    if (!unit->du_TxDesc || !unit->du_RxDesc || !unit->du_TxBuf || !unit->du_RxBuf) {
        D(bug("[dwmac] Memory allocation failed\n"));
        FreeVec(unit);
        return TRUE;
    }

    /* Read MAC from hardware (set by firmware) */
    if (unit->du_RegBase) {
        ULONG hi = AROS_LE2LONG(*(volatile ULONG *)(unit->du_RegBase + MAC_ADDR_HIGH));
        ULONG lo = AROS_LE2LONG(*(volatile ULONG *)(unit->du_RegBase + MAC_ADDR_LOW));
        unit->du_DevAddr[0] = lo & 0xFF;
        unit->du_DevAddr[1] = (lo >> 8) & 0xFF;
        unit->du_DevAddr[2] = (lo >> 16) & 0xFF;
        unit->du_DevAddr[3] = (lo >> 24) & 0xFF;
        unit->du_DevAddr[4] = hi & 0xFF;
        unit->du_DevAddr[5] = (hi >> 8) & 0xFF;
        CopyMem(unit->du_DevAddr, unit->du_OrgAddr, ETH_ADDRSIZE);
    }

    LIBBASE->du_Units[0] = unit;
    LIBBASE->du_UnitCount = 1;

    D(bug("[dwmac] Unit created (waiting for RP1 BAR1)\n"));

    return TRUE;
}

ADD2INITLIB(dwmac_Init, 0)
