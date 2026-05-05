/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: AArch64 platform probe framework.
          Iterates registered platform probes to find matching SoC.
          Follows the same pattern as arch/arm-native/kernel/platform_init.c.
*/

#include "kernel_intern.h"

/*
 * Platform probe function type.
 * Returns non-zero if this platform matches.
 */
typedef int (*platform_probe_func)(struct AARCH64_Implementation *, struct TagItem *);

extern int bcm2711_probe(struct AARCH64_Implementation *impl, struct TagItem *msg);
extern int bcm2712_probe(struct AARCH64_Implementation *impl, struct TagItem *msg);

/*
 * Platform probe table — try RPi5 first (Cortex-A76), then RPi4 (Cortex-A72).
 */
static platform_probe_func platform_probes[] = {
    bcm2712_probe,
    bcm2711_probe,
    (platform_probe_func)0    /* terminator */
};

void platform_Init(struct AARCH64_Implementation *impl, struct TagItem *msg)
{
    int i;
    for (i = 0; platform_probes[i]; i++)
    {
        if (platform_probes[i](impl, msg))
            return;
    }
}
