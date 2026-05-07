#include <config.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/kernel.h>
#include "library.h"
#include "DriverData.h"
#define DOSBase (*(struct DosLibrary **) &RPiI2SBase->dosbase)

APTR KernelBase = NULL;

BOOL DriverInit(struct DriverBase *AHIsubBase) {
    struct RPiI2SBase *RPiI2SBase = (struct RPiI2SBase *)AHIsubBase;
    DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 37);
    if (!DOSBase) return FALSE;
    KernelBase = OpenResource("kernel.resource");
    if (!KernelBase) return FALSE;
    RPiI2SBase->periiobase = KrnGetSystemAttr(KATTR_PeripheralBase);
    if (!RPiI2SBase->periiobase) return FALSE;
    return TRUE;
}

VOID DriverCleanup(struct DriverBase *AHIsubBase) {
    struct RPiI2SBase *RPiI2SBase = (struct RPiI2SBase *)AHIsubBase;
    CloseLibrary((struct Library *)DOSBase);
}

/* Stub for AROS symbol set handler (not needed for static AHI drivers) */
int __LIBS__symbol_set_handler_missing = 0;
