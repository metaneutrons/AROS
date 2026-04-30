/*
 * startup.c - CocoaGfx HIDD registration for AROS on macOS Apple Silicon
 *
 * Built as a simple executable (like the SDL monitor driver).
 * Loaded via LoadSeg + CreateNewProcTags from boot.c.
 */

#include <aros/debug.h>
#include <aros/kernel.h>
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

int __nocommandline = 1;

int main(void)
{
    struct GfxBase *GfxBase;
    struct Library *OOPBase;
    struct Library *UtilityBase;
    LONG err;

    bug("[CocoaGfx] Starting\n");

    /* Get HostIFace from kernel boot tags */
    {
        APTR KernelBase = OpenResource("kernel.resource");
        if (KernelBase) {
            struct TagItem *tags = KrnGetBootInfo();
            if (tags) {
                struct TagItem *tag = FindTagItem(KRN_HostInterface, tags);
                if (tag)
                    HostIFace = (struct HostInterface *)tag->ti_Data;
            }
        }
    }

    bug("[CocoaGfx] HostIFace=%p\n", HostIFace);
    if (!HostIFace || !HostIFace->cocoa_fb_base) {
        bug("[CocoaGfx] No Cocoa framebuffer!\n");
        bug("[CocoaGfx] No Cocoa framebuffer!\n");
        return 20;
    }

    xsd.fb_base   = HostIFace->cocoa_fb_base;
    xsd.fb_width  = HostIFace->cocoa_fb_width;
    xsd.fb_height = HostIFace->cocoa_fb_height;
    xsd.fb_pitch  = HostIFace->cocoa_fb_pitch;
    xsd.iface     = HostIFace;

    bug("[CocoaGfx] FB: %dx%d @ %p pitch=%d\n",
        xsd.fb_width, xsd.fb_height, xsd.fb_base, xsd.fb_pitch);

    OOPBase = OpenLibrary("oop.library", 0);
    if (!OOPBase) return 20;

    UtilityBase = OpenLibrary("utility.library", 0);
    if (!UtilityBase) { CloseLibrary(OOPBase); return 20; }

    /* Obtain attribute bases */
    __IMeta        = OOP_ObtainAttrBase(IID_Meta);
    __IHidd        = OOP_ObtainAttrBase(IID_Hidd);
    __IHidd_BitMap = OOP_ObtainAttrBase(IID_Hidd_BitMap);
    __IHidd_Gfx   = OOP_ObtainAttrBase(IID_Hidd_Gfx);
    __IHidd_Sync  = OOP_ObtainAttrBase(IID_Hidd_Sync);
    __IHidd_PixFmt= OOP_ObtainAttrBase(IID_Hidd_PixFmt);

    xsd.hiddBitMapAttrBase = __IHidd_BitMap;
    xsd.hiddGfxAttrBase    = __IHidd_Gfx;
    xsd.hiddSyncAttrBase   = __IHidd_Sync;
    xsd.hiddPixFmtAttrBase = __IHidd_PixFmt;

    if (!xsd.hiddBitMapAttrBase || !xsd.hiddGfxAttrBase) {
        bug("[CocoaGfx] Failed to obtain attr bases\n");
        goto fail;
    }

    /* Create GFX class */
    {
        struct TagItem tags[] = {
            { aMeta_SuperID,        (IPTR)CLID_Hidd_Gfx },
            { aMeta_InterfaceDescr, (IPTR)CocoaGfx_ifdescr },
            { aMeta_ID,             (IPTR)"hidd.gfx.cocoa" },
            { aMeta_InstSize,       0 },
            { TAG_DONE, 0 }
        };
        xsd.gfxclass = OOP_NewObject(NULL, CLID_HiddMeta, tags);
        if (!xsd.gfxclass) {
            bug("[CocoaGfx] Failed to create GFX class\n");
            goto fail;
        }
        xsd.gfxclass->UserData = &xsd;
    }

    /* Create BitMap class */
    {
        struct TagItem tags[] = {
            { aMeta_SuperID,        (IPTR)CLID_Hidd_BitMap },
            { aMeta_InterfaceDescr, (IPTR)CocoaBM_ifdescr },
            { aMeta_InstSize,       sizeof(struct CocoaBMData) },
            { TAG_DONE, 0 }
        };
        xsd.bmclass = OOP_NewObject(NULL, CLID_HiddMeta, tags);
        if (!xsd.bmclass) {
            bug("[CocoaGfx] Failed to create BitMap class\n");
            goto fail;
        }
        xsd.bmclass->UserData = &xsd;
    }

    /* Register GFX class as public */
    OOP_AddClass(xsd.gfxclass);

    /* Register with graphics.library */
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 41);
    if (!GfxBase) {
        bug("[CocoaGfx] Can't open graphics.library\n");
        goto fail;
    }

    err = AddDisplayDriverA(xsd.gfxclass, NULL, NULL);
    bug("[CocoaGfx] AddDisplayDriverA = %d\n", (int)err);
    CloseLibrary((struct Library *)GfxBase);

    if (err) goto fail;

    /* Stay resident */
    bug("[CocoaGfx] Driver registered, staying resident\n");
    {
        struct Process *me = (struct Process *)FindTask(NULL);
        if (me->pr_CLI) {
            struct CommandLineInterface *cli = BADDR(me->pr_CLI);
            cli->cli_Module = BNULL;
        } else {
            me->pr_SegList = BNULL;
        }
    }
    return 0;

fail:
    if (xsd.bmclass) OOP_DisposeObject((OOP_Object *)xsd.bmclass);
    if (xsd.gfxclass) OOP_DisposeObject((OOP_Object *)xsd.gfxclass);
    CloseLibrary(UtilityBase);
    CloseLibrary(OOPBase);
    return 20;
}
