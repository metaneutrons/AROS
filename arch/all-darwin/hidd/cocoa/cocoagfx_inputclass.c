/*
 * cocoagfx_inputclass.c - Mouse and Keyboard input for Cocoa HIDD
 *
 * Polls the ring buffer in HostInterface and fires callbacks to input.device.
 */

#include <aros/debug.h>
#include <hidd/gfx.h>
#include <hidd/input.h>
#include <hidd/mouse.h>
#include <hidd/keyboard.h>
#include <oop/oop.h>
#include <proto/exec.h>
#include <proto/oop.h>
#include <proto/utility.h>
#include <devices/timer.h>

#include "cocoa_intern.h"
#include "hostinterface.h"

/* ======== Mouse class ======== */

struct CocoaMouseData {
    void (*callback)(void *data, struct pHidd_Mouse_Event *ev);
    void *callbackdata;
};

OOP_Object *CocoaMouse__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    if (o) {
        struct CocoaMouseData *data = OOP_INST_DATA(cl, o);
        data->callback = (void *)GetTagData(aHidd_Input_IrqHandler, 0, msg->attrList);
        data->callbackdata = (void *)GetTagData(aHidd_Input_IrqHandlerData, 0, msg->attrList);
    }
    return o;
}

static struct OOP_MethodDescr CocoaMouse_Root_descr[] = {
    { (OOP_MethodFunc)CocoaMouse__Root__New, moRoot_New },
    { NULL, 0 }
};

struct OOP_InterfaceDescr CocoaMouse_ifdescr[] = {
    { CocoaMouse_Root_descr, IID_Root, 1 },
    { NULL, NULL }
};

/* ======== Keyboard class ======== */

struct CocoaKbdData {
    void (*callback)(void *data, UWORD keycode);
    void *callbackdata;
};

OOP_Object *CocoaKbd__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    if (o) {
        struct CocoaKbdData *data = OOP_INST_DATA(cl, o);
        data->callback = (void *)GetTagData(aHidd_Input_IrqHandler, 0, msg->attrList);
        data->callbackdata = (void *)GetTagData(aHidd_Input_IrqHandlerData, 0, msg->attrList);
    }
    return o;
}

static struct OOP_MethodDescr CocoaKbd_Root_descr[] = {
    { (OOP_MethodFunc)CocoaKbd__Root__New, moRoot_New },
    { NULL, 0 }
};

struct OOP_InterfaceDescr CocoaKbd_ifdescr[] = {
    { CocoaKbd_Root_descr, IID_Root, 1 },
    { NULL, NULL }
};

/* ======== Event polling ======== */

static OOP_Object *g_mouseobj = NULL;
static OOP_Object *g_kbdobj = NULL;

void cocoa_input_poll(struct HostInterface *hif)
{
    if (!hif) return;

    while (hif->cocoa_event_read != hif->cocoa_event_write) {
        typeof(hif->cocoa_events[0]) *ev = &hif->cocoa_events[hif->cocoa_event_read];

        if (g_mouseobj && (ev->type == COCOA_EVENT_MOUSE_MOVE ||
                           ev->type == COCOA_EVENT_MOUSE_PRESS ||
                           ev->type == COCOA_EVENT_MOUSE_RELEASE)) {
            struct CocoaMouseData *md = OOP_INST_DATA(OOP_OCLASS(g_mouseobj), g_mouseobj);
            if (md->callback) {
                struct pHidd_Mouse_Event me;
                me.x = ev->x;
                me.y = ev->y;
                if (ev->type == COCOA_EVENT_MOUSE_MOVE) {
                    me.type = vHidd_Mouse_Motion;
                    me.button = vHidd_Mouse_NoButton;
                } else {
                    me.type = (ev->type == COCOA_EVENT_MOUSE_PRESS) ?
                              vHidd_Mouse_Press : vHidd_Mouse_Release;
                    me.button = (ev->button == 1) ? vHidd_Mouse_Button1 :
                                (ev->button == 2) ? vHidd_Mouse_Button2 :
                                vHidd_Mouse_Button3;
                }
                md->callback(md->callbackdata, &me);
            }
        }

        if (g_kbdobj && (ev->type == COCOA_EVENT_KEY_PRESS ||
                         ev->type == COCOA_EVENT_KEY_RELEASE)) {
            struct CocoaKbdData *kd = OOP_INST_DATA(OOP_OCLASS(g_kbdobj), g_kbdobj);
            if (kd->callback) {
                UWORD code = ev->keycode;
                if (ev->type == COCOA_EVENT_KEY_RELEASE)
                    code |= 0x8000; /* IECODE_UP_PREFIX */
                kd->callback(kd->callbackdata, code);
            }
        }

        hif->cocoa_event_read = (hif->cocoa_event_read + 1) % COCOA_EVENT_RING_SIZE;
    }
}

void cocoa_input_set_objects(OOP_Object *mouse, OOP_Object *kbd)
{
    g_mouseobj = mouse;
    g_kbdobj = kbd;
}
