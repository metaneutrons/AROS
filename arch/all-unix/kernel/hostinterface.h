#ifndef _HOSTINTERFACE_H
#define _HOSTINTERFACE_H

#include <stdint.h>

#define HOSTINTERFACE_VERSION 6

struct HostInterface
{
    char *System;
    unsigned int Version;

    void *(*hostlib_Open)(const char *, char **);
    int   (*hostlib_Close)(void *, char **);
    void *(*hostlib_GetPointer)(void *, const char *, char **);
    int   (*KPutC)(int chr);
    int   (*host_GetTime)(int, uint64_t *, uint64_t *);
    struct MinList **ModListPtr;
    void  (*jit_write_protect)(int enabled);  /* W^X toggle for MAP_JIT (NULL if not needed) */

    /* Cocoa display (darwin-aarch64 only, NULL otherwise) */
    void *(*cocoa_display_init)(int w, int h);
    int   (*cocoa_display_get_pitch)(void);
    void  (*cocoa_display_refresh)(void);
    void  (*cocoa_runloop_step)(void);
    void  *cocoa_fb_base;   /* Framebuffer pixel base address */
    int    cocoa_fb_width;
    int    cocoa_fb_height;
    int    cocoa_fb_pitch;
};

#endif /* !_HOSTINTERFACE_H */
