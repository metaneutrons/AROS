/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: AArch64 kernel internal definitions.
          Modeled after arch/arm-native/kernel/kernel_intern.h.
*/

#ifndef KERNEL_INTERN_H_
#define KERNEL_INTERN_H_

#include <aros/libcall.h>
#include <inttypes.h>
#include <exec/lists.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <utility/tagitem.h>
#include <stdio.h>
#include <stdarg.h>

#include "kernel_aarch64.h"

#undef KernelBase
struct KernelBase;

/* Device tree helpers */
void dt_set_root(void *r);
void *dt_find_node(char *key);
void *dt_find_node_by_phandle(uint32_t phandle);
void *dt_find_property(void *key, char *propname);
int dt_get_prop_len(void *prop);
void *dt_get_prop_value(void *prop);

/* CPU/platform init */
void cpu_Init(struct AARCH64_Implementation *, struct TagItem *);
void platform_Init(struct AARCH64_Implementation *, struct TagItem *);

void core_SetupIntr(void);
void *KrnAddSysTimerHandler(struct KernelBase *);

/* Tag helpers */
intptr_t krnGetTagData(Tag tagValue, intptr_t defaultVal, const struct TagItem *tagList);
struct TagItem *krnFindTagItem(Tag tagValue, const struct TagItem *tagList);
struct TagItem *krnNextTagItem(const struct TagItem **tagListPtr);

struct KernelBase *getKernelBase(void);

/* Debug */
#ifdef bug
#undef bug
#endif
#ifdef D
#undef D
#endif

#define DEBUG 0

#if DEBUG
#define D(x) x
#define DALLOCMEM(x)
#else
#define D(x)
#define DALLOCMEM(x)
#endif

AROS_LD2(int, KrnBug,
         AROS_LDA(const char *, format, A0),
         AROS_LDA(va_list, args, A1),
         struct KernelBase *, KernelBase, 12, Kernel);

static inline void bug(const char *format, ...)
{
    struct KernelBase *kbase = getKernelBase();
    va_list args;
    va_start(args, format);
    AROS_SLIB_ENTRY(KrnBug, Kernel, 12)(format, args, kbase);
    va_end(args);
}


#endif /* KERNEL_INTERN_H_ */
