/*
    Copyright (C) 1995-2014, The AROS Development Team. All rights reserved.
*/

#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__APPLE__) && defined(__aarch64__)
/* Forward-declare pthread to avoid conflict with AROS pthread.h */
typedef struct _opaque_pthread_t *pthread_t_host;
typedef struct _opaque_pthread_attr_t *pthread_attr_t_host;
int pthread_create(pthread_t_host *, const pthread_attr_t_host *, void *(*)(void *), void *);
int pthread_detach(pthread_t_host);
#endif

/* These macros are defined in both UNIX and AROS headers. Get rid of warnings. */
#undef __pure
#undef __const
#undef __pure2
#undef __deprecated

#include <aros/kernel.h>
#include <runtime.h>

#include "kickstart.h"
#include "platform.h"
#include "hostinterface.h"

#if defined(__APPLE__) && defined(__aarch64__)

extern void *cocoa_display_init(int width, int height);
extern int   cocoa_display_get_pitch(void);
extern void  cocoa_runloop_step(void);

struct kick_args {
    kernel_entry_fun_t addr;
    struct TagItem *msg;
    int result;
};

static void *kernel_thread(void *arg)
{
    struct kick_args *ka = (struct kick_args *)arg;
    fprintf(stderr, "[Bootstrap] Kernel thread started, entering at %p...\n", ka->addr);
    Host_PreBoot();
    ka->result = ka->addr(ka->msg, AROS_BOOT_MAGIC);
    fprintf(stderr, "[Bootstrap] Kernel returned %d\n", ka->result);
    exit(ka->result);
    return NULL;
}

int kick(kernel_entry_fun_t addr, struct TagItem *msg)
{
    struct kick_args ka = { addr, msg, 0 };
    pthread_t_host tid;

    /* Init Cocoa display FIRST (must happen on main thread) */
    {
        extern void *HostIFace;
        struct HostInterface *hi = (struct HostInterface *)HostIFace;
        void *fb = cocoa_display_init(640, 480);
        hi->cocoa_fb_base = fb;
        hi->cocoa_fb_width = 640;
        hi->cocoa_fb_height = 480;
        hi->cocoa_fb_pitch = cocoa_display_get_pitch();
        fprintf(stderr, "[Bootstrap] Framebuffer at %p, %dx%d pitch=%d\n",
                fb, 640, 480, hi->cocoa_fb_pitch);
    }

    /* NOW start the kernel on a background thread */
    fprintf(stderr, "[Bootstrap] Starting kernel on background thread...\n");
    pthread_create(&tid, NULL, kernel_thread, &ka);
    pthread_detach(tid);

    /* Main thread: pump the Cocoa run loop forever */
    for (;;)
        cocoa_runloop_step();

    /* Not reached */
    return ka.result;
}

#else /* !darwin-aarch64 */

#define D(x)

int kick(kernel_entry_fun_t addr, struct TagItem *msg)
{
    int i;

    /* DEBUG: bypass fork to allow lldb debugging */
    fprintf(stderr, "[Bootstrap] Entering kernel at %p (no fork)...\n", addr);
    Host_PreBoot();
    i = addr(msg, AROS_BOOT_MAGIC);
    fprintf(stderr, "[Bootstrap] Kernel returned %d\n", i);
    return i;
}

#endif
