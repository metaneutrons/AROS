#ifdef __cplusplus
extern "C" {
#endif

#ifndef AROS_MESA_POSIX_H
#include <sys/types.h>
#define AROS_MESA_POSIX_H
/* Extra POSIX declarations for Mesa on AROS */
#include <stddef.h>
int posix_memalign(void **ptr, size_t align, size_t size);
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int sched_yield(void);
