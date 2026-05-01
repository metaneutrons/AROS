/*
 * startup.c - CocoaGfx HIDD registration for AROS on macOS Apple Silicon
 *
 * Built as a simple executable (like the SDL monitor driver).
 * Loaded via LoadSeg + CreateNewProcTags from boot.c.
 */

#define DEBUG 0
#include <aros/debug.h>


#include <aros/kernel.h>
#include <aros/symbolsets.h>
#include <aros/asmcall.h>
#include <aros/startup.h>
#include <dos/dosextens.h>
#include <dos/dos.h>
#include <hidd/gfx.h>
#include <hidd/hidd.h>
#include <oop/oop.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/kernel.h>
#include <proto/oop.h>
#include <proto/utility.h>

#include "cocoa_intern.h"

/* HostInterface definition */
#include "hostinterface.h"

#include <proto/kernel.h>

/* Retrieved at runtime from kernel.resource */
struct HostInterface *HostIFace;
extern struct OOP_InterfaceDescr CocoaGfx_ifdescr[];
extern struct OOP_InterfaceDescr CocoaBM_ifdescr[];

static struct CocoaGfx_staticdata xsd;
OOP_AttrBase __IMeta;
OOP_AttrBase __IHidd;
OOP_AttrBase __IHidd_BitMap;
OOP_AttrBase __IHidd_Gfx;
OOP_AttrBase __IHidd_Sync;
OOP_AttrBase __IHidd_PixFmt;
OOP_AttrBase __IHidd_ChunkyBM;

int __nocommandline = 1;
int __noinitexitsets = 1;

/* These are normally provided by initexitsets.o which we exclude via __noinitexitsets. */
THIS_PROGRAM_HANDLES_SYMBOLSET(LIBS)
THIS_PROGRAM_HANDLES_SYMBOLSET(INIT)
THIS_PROGRAM_HANDLES_SYMBOLSET(EXIT)
THIS_PROGRAM_HANDLES_SYMBOLSET(CTORS)
THIS_PROGRAM_HANDLES_SYMBOLSET(DTORS)
THIS_PROGRAM_HANDLES_SYMBOLSET(INIT_ARRAY)
THIS_PROGRAM_HANDLES_SYMBOLSET(FINI_ARRAY)
DEFINESET(LIBS);
DEFINESET(INIT);
DEFINESET(EXIT);
DEFINESET(CTORS);
DEFINESET(DTORS);
DEFINESET(INIT_ARRAY);
DEFINESET(FINI_ARRAY);
void __attribute__((weak)) __register_frame(void *begin) {}
void __attribute__((weak)) __deregister_frame(void *begin) {}

/* Provide our own minimal startup entry that bypasses PROGRAM_ENTRIES.
 * The standard startup hangs because task hooks or other init code blocks. */
LONG __startup_error;

__startup AROS_PROCH(__startup_entry, argstr, argsize, sysBase)
{
    AROS_PROCFUNC_INIT

    /* Set the global SysBase from the parameter passed by CreateNewProc */
    SysBase = sysBase;

    extern int main(void);
    return main();

    AROS_PROCFUNC_EXIT
}

int __startup_error_storage;
int *__startup_error_ptr = &__startup_error_storage;

APTR KernelBase;

int main(void)
{
    /* Open libraries - these set the global variables used by OOP macros */
    OOPBase = OpenLibrary("oop.library", 0);
    UtilityBase = (APTR)OpenLibrary("utility.library", 0);
    KernelBase = OpenResource("kernel.resource");

    if (KernelBase && OOPBase && UtilityBase) {
        struct TagItem *tags = KrnGetBootInfo();
        if (tags) {
            struct TagItem *tag = FindTagItem(KRN_HostInterface, tags);
            if (tag) {
                struct HostInterface *hif = (struct HostInterface *)tag->ti_Data;

                if (hif && hif->cocoa_fb_base) {
                        struct GfxBase *GfxBase;
                        LONG err;

                        HostIFace = hif;
                        xsd.fb_base   = hif->cocoa_fb_base;
                        xsd.fb_width  = hif->cocoa_fb_width;
                        xsd.fb_height = hif->cocoa_fb_height;
                        xsd.fb_pitch  = hif->cocoa_fb_pitch;
                        xsd.iface     = hif;

                        __IMeta        = OOP_ObtainAttrBase(IID_Meta);
                        __IHidd        = OOP_ObtainAttrBase(IID_Hidd);
                        __IHidd_BitMap = OOP_ObtainAttrBase(IID_Hidd_BitMap);
                        __IHidd_Gfx   = OOP_ObtainAttrBase(IID_Hidd_Gfx);
                        __IHidd_Sync  = OOP_ObtainAttrBase(IID_Hidd_Sync);
                        __IHidd_PixFmt= OOP_ObtainAttrBase(IID_Hidd_PixFmt);
                        __IHidd_ChunkyBM = OOP_ObtainAttrBase(IID_Hidd_ChunkyBM);

                        xsd.hiddBitMapAttrBase = __IHidd_BitMap;
                        xsd.hiddGfxAttrBase    = __IHidd_Gfx;
                        xsd.hiddSyncAttrBase   = __IHidd_Sync;
                        xsd.hiddPixFmtAttrBase = __IHidd_PixFmt;

                        if (__IHidd_BitMap && __IHidd_Gfx) {
                            struct TagItem gtags[] = {
                                { aMeta_SuperID,        (IPTR)CLID_Hidd_Gfx },
                                { aMeta_InterfaceDescr, (IPTR)CocoaGfx_ifdescr },
                                { aMeta_ID,             (IPTR)"hidd.gfx.cocoa" },
                                { aMeta_InstSize,       0 },
                                { TAG_DONE, 0 }
                            };
                            struct TagItem btags[] = {
                                { aMeta_SuperID,        (IPTR)CLID_Hidd_ChunkyBM },
                                { aMeta_InterfaceDescr, (IPTR)CocoaBM_ifdescr },
                                { aMeta_InstSize,       sizeof(struct CocoaBMData) },
                                { TAG_DONE, 0 }
                            };

                            xsd.gfxclass = OOP_NewObject(NULL, CLID_HiddMeta, gtags);
                            if (xsd.gfxclass) {
                                xsd.gfxclass->UserData = &xsd;
                                xsd.bmclass = OOP_NewObject(NULL, CLID_HiddMeta, btags);
                                if (xsd.bmclass) {
                                    xsd.bmclass->UserData = &xsd;
                                    OOP_AddClass(xsd.gfxclass);

                                    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 41);
                                    if (GfxBase) {
                                        err = AddDisplayDriverA(xsd.gfxclass, NULL, NULL);
                                        CloseLibrary((struct Library *)GfxBase);
                                    }
                                }
                            }
                        }
                }
            }
        }
    }

    /* Stay resident - don't exit or classes will be freed */
    Wait(0);
    return 0;
}
