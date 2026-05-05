/*
 * POSIX/Linux compatibility stubs for Mesa 26 on AROS
 *
 * Mesa expects various POSIX functions that don't exist on AROS.
 * These stubs provide minimal implementations sufficient for
 * single-threaded GPU driver operation.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ============================================================
 * pthreads — single-threaded stubs
 * ============================================================ */

typedef int pthread_mutex_t;
typedef int pthread_mutexattr_t;
typedef int pthread_once_t;
typedef int pthread_key_t;
typedef unsigned long pthread_t;
typedef int pthread_cond_t;
typedef int pthread_condattr_t;

int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) { (void)m; (void)a; return 0; }
int pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_mutex_lock(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_mutex_unlock(pthread_mutex_t *m) { (void)m; return 0; }
int pthread_mutexattr_init(pthread_mutexattr_t *a) { (void)a; return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int t) { (void)a; (void)t; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void)a; return 0; }

int pthread_once(pthread_once_t *o, void (*f)(void)) { if (!*o) { f(); *o = 1; } return 0; }

static void *tls_values[64];
int pthread_key_create(pthread_key_t *k, void (*d)(void *)) { static int next = 0; *k = next++; (void)d; return 0; }
int pthread_key_delete(pthread_key_t k) { (void)k; return 0; }
void *pthread_getspecific(pthread_key_t k) { return (k < 64) ? tls_values[k] : NULL; }
int pthread_setspecific(pthread_key_t k, const void *v) { if (k < 64) tls_values[k] = (void *)v; return 0; }

pthread_t pthread_self(void) { return 1; }
int pthread_equal(pthread_t a, pthread_t b) { return a == b; }

int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) { (void)c; (void)a; return 0; }
int pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { (void)c; (void)m; return 0; }
int pthread_cond_signal(pthread_cond_t *c) { (void)c; return 0; }
int pthread_cond_broadcast(pthread_cond_t *c) { (void)c; return 0; }

/* ============================================================
 * mmap — map to AllocVec
 * ============================================================ */

#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)

void *mmap(void *addr, size_t len, int prot, int flags, int fd, long offset)
{
    (void)addr; (void)prot; (void)flags; (void)fd; (void)offset;
    void *p = AllocVec(len, MEMF_CLEAR | MEMF_PUBLIC);
    return p ? p : MAP_FAILED;
}

int munmap(void *addr, size_t len)
{
    (void)len;
    if (addr && addr != MAP_FAILED)
        FreeVec(addr);
    return 0;
}

int mprotect(void *addr, size_t len, int prot)
{
    (void)addr; (void)len; (void)prot;
    return 0;
}

/* ============================================================
 * clock — ARM Generic Timer
 * ============================================================ */

#define CLOCK_MONOTONIC 1

struct timespec {
    long tv_sec;
    long tv_nsec;
};

int clock_gettime(int clk, struct timespec *ts)
{
    uint64_t cnt, freq;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    ts->tv_sec = cnt / freq;
    ts->tv_nsec = ((cnt % freq) * 1000000000ULL) / freq;
    return 0;
}

/* ============================================================
 * dlopen — not supported (static linking)
 * ============================================================ */

void *dlopen(const char *f, int flags) { (void)f; (void)flags; return NULL; }
void *dlsym(void *h, const char *s) { (void)h; (void)s; return NULL; }
int dlclose(void *h) { (void)h; return 0; }
char *dlerror(void) { return "dlopen not supported on AROS"; }

/* ============================================================
 * DRM — stubs (we use v3d_ioctl_aros instead)
 * ============================================================ */

int drmIoctl(int fd, unsigned long req, void *arg) { (void)fd; (void)req; (void)arg; return -1; }
int drmSyncobjCreate(int fd, uint32_t f, uint32_t *h) { *h = 1; (void)fd; (void)f; return 0; }
int drmSyncobjDestroy(int fd, uint32_t h) { (void)fd; (void)h; return 0; }
int drmSyncobjWait(int fd, uint32_t *h, uint32_t c, int64_t t, uint32_t f, uint32_t *r)
    { (void)fd; (void)h; (void)c; (void)t; (void)f; (void)r; return 0; }
int drmPrimeHandleToFD(int fd, uint32_t h, uint32_t f, int *pfd)
    { (void)fd; (void)h; (void)f; *pfd = -1; return -1; }
int drmPrimeFDToHandle(int fd, int pfd, uint32_t *h)
    { (void)fd; (void)pfd; *h = 0; return -1; }

/* ============================================================
 * Misc POSIX
 * ============================================================ */

int getpagesize(void) { return 4096; }
long sysconf(int name) { (void)name; return 4096; }

struct sysinfo { unsigned long totalram; };
int sysinfo(struct sysinfo *i) { i->totalram = 1024*1024*1024; return 0; }

int posix_memalign(void **ptr, size_t align, size_t size)
{
    /* Simple implementation — over-allocate and align */
    void *p = AllocVec(size + align, MEMF_PUBLIC);
    if (!p) return -1;
    *ptr = (void *)(((uintptr_t)p + align - 1) & ~(align - 1));
    return 0;
}
