/*
 *  RPiHDMI AHI driver initialization
 */

#include <config.h>

#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/kernel.h>

#include "library.h"
#include "DriverData.h"
#include "rpihdmi-hwaccess.h"

APTR KernelBase = NULL;

/******************************************************************************
** Custom driver init *********************************************************
******************************************************************************/

BOOL DriverInit(struct DriverBase *AHIsubBase)
{
    struct RPiHDMIBase *RPiHDMIBase = (struct RPiHDMIBase *) AHIsubBase;

    DOSBase = (struct DosLibrary *) OpenLibrary(DOSNAME, 37);

    if (DOSBase == NULL) {
        Req("Unable to open 'dos.library' version 37.\n");
        return FALSE;
    }

    KernelBase = OpenResource("kernel.resource");

    if (KernelBase == NULL) {
        Req("Unable to open 'kernel.resource'.\n");
        return FALSE;
    }

    RPiHDMIBase->periiobase = KrnGetSystemAttr(KATTR_PeripheralBase);

    if (RPiHDMIBase->periiobase == 0) {
        Req("No BCM283x/BCM2711 peripheral base found.\n");
        return FALSE;
    }

    /*
     * Detect SoC variant from peripheral base address.
     * BCM2711 (RPi4) uses 0xFE000000, all earlier SoCs use lower addresses.
     */
    if (RPiHDMIBase->periiobase >= 0xFE000000)
        RPiHDMIBase->variant = VARIANT_BCM2711;
    else
        RPiHDMIBase->variant = VARIANT_BCM2835;

    return TRUE;
}


/******************************************************************************
** Custom driver clean-up *****************************************************
******************************************************************************/

VOID DriverCleanup(struct DriverBase *AHIsubBase)
{
    struct RPiHDMIBase *RPiHDMIBase = (struct RPiHDMIBase *) AHIsubBase;

    CloseLibrary((struct Library *) DOSBase);
}
