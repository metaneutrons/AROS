/*
 * cocoagfx_bitmapclass.c - Cocoa BitMap HIDD for AROS on macOS Apple Silicon
 *
 * This bitmap class wraps the IOSurface framebuffer memory directly.
 */

#include <aros/debug.h>
#include <hidd/gfx.h>
#include <oop/oop.h>
#include <proto/exec.h>
#include <proto/oop.h>
#include <proto/utility.h>
#include <string.h>

#include "cocoa_intern.h"
#include "hostinterface.h"

extern struct HostInterface *HostIFace;

/* ======== BitMap::New ======== */

OOP_Object *CocoaBM__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    if (o) {
        struct CocoaBMData *data = OOP_INST_DATA(cl, o);
        struct CocoaGfx_staticdata *csd = CSD(cl);
        BOOL is_fb = GetTagData(aHidd_BitMap_FrameBuffer, FALSE, msg->attrList);

        data->width  = GetTagData(aHidd_BitMap_Width,  csd->fb_width,  msg->attrList);
        data->height = GetTagData(aHidd_BitMap_Height, csd->fb_height, msg->attrList);
        data->bpp    = 4;

        if (is_fb) {
            /* Use the IOSurface framebuffer directly */
            data->pixels = csd->fb_base;
            data->pitch  = csd->fb_pitch;
            data->is_fb  = TRUE;
            bug("[CocoaBM] Framebuffer bitmap: %dx%d @ %p pitch=%d\n",
                data->width, data->height, data->pixels, data->pitch);
        } else {
            /* Allocate offscreen buffer */
            data->pitch  = data->width * data->bpp;
            data->pixels = AllocVec(data->pitch * data->height, MEMF_ANY | MEMF_CLEAR);
            data->is_fb  = FALSE;
            if (!data->pixels) {
                OOP_MethodID mid = OOP_GetMethodID(IID_Root, moRoot_Dispose);
                OOP_CoerceMethod(cl, o, (OOP_Msg)&mid);
                return NULL;
            }
        }
    }
    return o;
}

/* ======== BitMap::Dispose ======== */

VOID CocoaBM__Root__Dispose(OOP_Class *cl, OOP_Object *o, OOP_Msg msg)
{
    struct CocoaBMData *data = OOP_INST_DATA(cl, o);

    if (data->pixels && !data->is_fb)
        FreeVec(data->pixels);

    OOP_DoSuperMethod(cl, o, msg);
}

/* ======== BitMap::Get ======== */

VOID CocoaBM__Root__Get(OOP_Class *cl, OOP_Object *o, struct pRoot_Get *msg)
{
    struct CocoaBMData *data = OOP_INST_DATA(cl, o);
    ULONG idx;

    if (IS_BITMAP_ATTR(msg->attrID, idx)) {
        switch (idx) {
        case aoHidd_BitMap_BytesPerRow:
            *msg->storage = data->pitch;
            return;
        }
    }
    OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

/* ======== BitMap::PutPixel ======== */

VOID CocoaBM__Hidd_BitMap__PutPixel(OOP_Class *cl, OOP_Object *o,
                                     struct pHidd_BitMap_PutPixel *msg)
{
    struct CocoaBMData *data = OOP_INST_DATA(cl, o);
    ULONG *row = (ULONG *)((UBYTE *)data->pixels + msg->y * data->pitch);
    row[msg->x] = msg->pixel;
}

/* ======== BitMap::GetPixel ======== */

HIDDT_Pixel CocoaBM__Hidd_BitMap__GetPixel(OOP_Class *cl, OOP_Object *o,
                                            struct pHidd_BitMap_GetPixel *msg)
{
    struct CocoaBMData *data = OOP_INST_DATA(cl, o);
    ULONG *row = (ULONG *)((UBYTE *)data->pixels + msg->y * data->pitch);
    return row[msg->x];
}

/* ======== BitMap::PutImage ======== */

VOID CocoaBM__Hidd_BitMap__PutImage(OOP_Class *cl, OOP_Object *o,
                                     struct pHidd_BitMap_PutImage *msg)
{
    struct CocoaBMData *data = OOP_INST_DATA(cl, o);

    if (msg->pixFmt == vHidd_StdPixFmt_Native || msg->pixFmt == vHidd_StdPixFmt_Native32) {
        /* Direct copy */
        UBYTE *src = msg->pixels;
        UBYTE *dst = (UBYTE *)data->pixels + msg->y * data->pitch + msg->x * data->bpp;
        ULONG copy_width = msg->width * data->bpp;
        int i;

        for (i = 0; i < msg->height; i++) {
            memcpy(dst, src, copy_width);
            src += msg->modulo;
            dst += data->pitch;
        }
    } else {
        /* Fall back to superclass for format conversion */
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    }
}

/* ======== BitMap::GetImage ======== */

VOID CocoaBM__Hidd_BitMap__GetImage(OOP_Class *cl, OOP_Object *o,
                                     struct pHidd_BitMap_GetImage *msg)
{
    struct CocoaBMData *data = OOP_INST_DATA(cl, o);

    if (msg->pixFmt == vHidd_StdPixFmt_Native || msg->pixFmt == vHidd_StdPixFmt_Native32) {
        UBYTE *src = (UBYTE *)data->pixels + msg->y * data->pitch + msg->x * data->bpp;
        UBYTE *dst = msg->pixels;
        ULONG copy_width = msg->width * data->bpp;
        int i;

        for (i = 0; i < msg->height; i++) {
            memcpy(dst, src, copy_width);
            src += data->pitch;
            dst += msg->modulo;
        }
    } else {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    }
}

/* ======== BitMap::FillRect ======== */

VOID CocoaBM__Hidd_BitMap__FillRect(OOP_Class *cl, OOP_Object *o,
                                     struct pHidd_BitMap_DrawRect *msg)
{
    struct CocoaBMData *data = OOP_INST_DATA(cl, o);
    HIDDT_Pixel pixel = GC_FG(msg->gc);
    int x, y;

    for (y = msg->minY; y <= msg->maxY; y++) {
        ULONG *row = (ULONG *)((UBYTE *)data->pixels + y * data->pitch);
        for (x = msg->minX; x <= msg->maxX; x++) {
            row[x] = pixel;
        }
    }
}

/* ======== BitMap::UpdateRect ======== */

VOID CocoaBM__Hidd_BitMap__UpdateRect(OOP_Class *cl, OOP_Object *o,
                                       struct pHidd_BitMap_UpdateRect *msg)
{
    struct CocoaBMData *data = OOP_INST_DATA(cl, o);

    /* Trigger Cocoa display refresh when the framebuffer is updated */
    if (data->is_fb && HostIFace && HostIFace->cocoa_display_refresh)
        HostIFace->cocoa_display_refresh();
}

/* ======== Method tables ======== */

static struct OOP_MethodDescr CocoaBM_Root_descr[] = {
    { (OOP_MethodFunc)CocoaBM__Root__New,     moRoot_New     },
    { (OOP_MethodFunc)CocoaBM__Root__Dispose, moRoot_Dispose },
    { (OOP_MethodFunc)CocoaBM__Root__Get,     moRoot_Get     },
    { NULL, 0 }
};

static struct OOP_MethodDescr CocoaBM_BitMap_descr[] = {
    { (OOP_MethodFunc)CocoaBM__Hidd_BitMap__PutPixel,   moHidd_BitMap_PutPixel   },
    { (OOP_MethodFunc)CocoaBM__Hidd_BitMap__GetPixel,   moHidd_BitMap_GetPixel   },
    { (OOP_MethodFunc)CocoaBM__Hidd_BitMap__PutImage,   moHidd_BitMap_PutImage   },
    { (OOP_MethodFunc)CocoaBM__Hidd_BitMap__GetImage,   moHidd_BitMap_GetImage   },
    { (OOP_MethodFunc)CocoaBM__Hidd_BitMap__FillRect,   moHidd_BitMap_FillRect   },
    { (OOP_MethodFunc)CocoaBM__Hidd_BitMap__UpdateRect, moHidd_BitMap_UpdateRect },
    { NULL, 0 }
};

struct OOP_InterfaceDescr CocoaBM_ifdescr[] = {
    { CocoaBM_Root_descr,   IID_Root,       3 },
    { CocoaBM_BitMap_descr, IID_Hidd_BitMap, 6 },
    { NULL, NULL }
};
