/*
 * POSIX compatibility for Mesa 26 on AROS — all implementations
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* pthreads */
static void *tls_values[256];
int pthread_mutex_init(void *m, const void *a) { (void)m; (void)a; return 0; }
int pthread_mutex_destroy(void *m) { (void)m; return 0; }
int pthread_mutex_lock(void *m) { (void)m; return 0; }
int pthread_mutex_unlock(void *m) { (void)m; return 0; }
int pthread_mutex_trylock(void *m) { (void)m; return 0; }
int pthread_mutex_timedlock(void *m, const void *t) { (void)m; (void)t; return 0; }
int pthread_mutexattr_init(void *a) { (void)a; return 0; }
int pthread_mutexattr_settype(void *a, int t) { (void)a; (void)t; return 0; }
int pthread_mutexattr_destroy(void *a) { (void)a; return 0; }
int pthread_rwlock_init(void *l, const void *a) { (void)l; (void)a; return 0; }
int pthread_rwlock_destroy(void *l) { (void)l; return 0; }
int pthread_rwlock_rdlock(void *l) { (void)l; return 0; }
int pthread_rwlock_wrlock(void *l) { (void)l; return 0; }
int pthread_rwlock_unlock(void *l) { (void)l; return 0; }
int pthread_once(int *o, void (*f)(void)) { if (o && !*o) { f(); *o = 1; } return 0; }
int pthread_key_create(unsigned int *k, void (*d)(void *)) { static unsigned int n = 0; *k = n++; (void)d; return 0; }
int pthread_key_delete(unsigned int k) { (void)k; return 0; }
void *pthread_getspecific(unsigned int k) { return (k < 256) ? tls_values[k] : NULL; }
int pthread_setspecific(unsigned int k, const void *v) { if (k < 256) tls_values[k] = (void *)v; return 0; }
unsigned long pthread_self(void) { return 1; }
int pthread_equal(unsigned long a, unsigned long b) { return a == b; }
int pthread_create(void *t, const void *a, void *(*f)(void*), void *arg) { (void)t; (void)a; (void)f; (void)arg; return -1; }
int pthread_join(unsigned long t, void **r) { (void)t; (void)r; return -1; }
int pthread_detach(unsigned long t) { (void)t; return 0; }
void pthread_exit(void *r) { (void)r; while(1); }
int pthread_cond_init(void *c, const void *a) { (void)c; (void)a; return 0; }
int pthread_cond_destroy(void *c) { (void)c; return 0; }
int pthread_cond_wait(void *c, void *m) { (void)c; (void)m; return 0; }
int pthread_cond_signal(void *c) { (void)c; return 0; }
int pthread_cond_broadcast(void *c) { (void)c; return 0; }
int pthread_cond_timedwait(void *c, void *m, const void *t) { (void)c; (void)m; (void)t; return 0; }
int pthread_condattr_init(void *a) { (void)a; return 0; }
int pthread_condattr_setclock(void *a, int c) { (void)a; (void)c; return 0; }
int pthread_condattr_destroy(void *a) { (void)a; return 0; }
int pthread_attr_init(void *a) { (void)a; return 0; }
int pthread_attr_destroy(void *a) { (void)a; return 0; }
int pthread_attr_setdetachstate(void *a, int s) { (void)a; (void)s; return 0; }
int pthread_barrier_init(void *b, const void *a, unsigned c) { (void)b; (void)a; (void)c; return 0; }
int pthread_barrier_destroy(void *b) { (void)b; return 0; }
int pthread_barrier_wait(void *b) { (void)b; return 0; }
int pthread_setaffinity_np(unsigned long t, size_t s, const void *c) { (void)t; (void)s; (void)c; return 0; }

/* semaphore */
int sem_init(int *s, int p, unsigned v) { (void)p; *s = v; return 0; }
int sem_destroy(int *s) { (void)s; return 0; }
int sem_wait(int *s) { (void)s; return 0; }
int sem_post(int *s) { (void)s; return 0; }
int sem_timedwait(int *s, const void *t) { (void)s; (void)t; return 0; }

/* mmap */
void *mmap(void *a, size_t l, int p, int f, int fd, long o) {
    (void)a; (void)p; (void)f; (void)fd; (void)o;
    return malloc(l);
}
int munmap(void *a, size_t l) { (void)l; free(a); return 0; }
int mprotect(void *a, size_t l, int p) { (void)a; (void)l; (void)p; return 0; }

/* clock */
int clock_gettime(int clk, void *ts) {
    uint64_t cnt, freq;
    long *t = (long *)ts;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cnt));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    t[0] = cnt / freq;
    t[1] = ((cnt % freq) * 1000000000ULL) / freq;
    (void)clk;
    return 0;
}

/* dl */
void *dlopen(const char *f, int fl) { (void)f; (void)fl; return NULL; }
void *dlsym(void *h, const char *s) { (void)h; (void)s; return NULL; }
int dlclose(void *h) { (void)h; return 0; }
char *dlerror(void) { return "not supported"; }

/* misc */
int getpagesize(void) { return 4096; }
long sysconf(int n) { (void)n; return 4096; }
int posix_memalign(void **ptr, size_t align, size_t size) {
    void *p = malloc(size + align);
    if (!p) return -1;
    *ptr = (void *)(((uintptr_t)p + align - 1) & ~(align - 1));
    return 0;
}
unsigned int sleep(unsigned int s) { (void)s; return 0; }
int usleep(unsigned int u) { (void)u; return 0; }
int pipe(int fd[2]) { fd[0] = fd[1] = -1; return -1; }
int fcntl(int fd, int cmd, ...) { (void)fd; (void)cmd; return -1; }
char *getenv(const char *n) { (void)n; return NULL; }
int setenv(const char *n, const char *v, int o) { (void)n; (void)v; (void)o; return 0; }
int unsetenv(const char *n) { (void)n; return 0; }

/* File I/O stubs */
int mkstemp(char *t) { (void)t; return -1; }
int ftruncate(int fd, long l) { (void)fd; (void)l; return -1; }
int asprintf(char **s, const char *fmt, ...) { *s = malloc(256); if (!*s) return -1; return 0; }
char *realpath(const char *p, char *r) { if (r) { strcpy(r, p ? p : ""); return r; } return NULL; }
int dup(int fd) { (void)fd; return -1; }

int mkstemps(char *t, int s) { (void)t; (void)s; return -1; }
int fchmod(int fd, unsigned int m) { (void)fd; (void)m; return 0; }
int symlink(const char *t, const char *l) { (void)t; (void)l; return -1; }
int readlink(const char *p, char *b, size_t s) { (void)p; (void)b; (void)s; return -1; }
int getpwuid_r(unsigned int uid, void *pwd, char *buf, size_t len, void **res) {
    (void)uid; (void)pwd; (void)buf; (void)len; *res = NULL; return -1;
}

#include <stdio.h>
FILE *fdopen(int fd, const char *mode) { (void)fd; (void)mode; return NULL; }
const char *util_get_process_name(void) { return "AROS"; }
FILE *open_memstream(char **p, size_t *s) { (void)p; (void)s; return NULL; }
int clock_nanosleep(int c, int f, const void *r, void *rm) { (void)c; (void)f; (void)r; (void)rm; return 0; }
int nanosleep(const void *r, void *rm) { (void)r; (void)rm; return 0; }
void *localtime_r(const void *t, void *r) { (void)t; return r; }
long syscall(long n, ...) { (void)n; return -1; }
int inotify_init1(int f) { (void)f; return -1; }
int inotify_add_watch(int fd, const char *p, unsigned int m) { (void)fd; (void)p; (void)m; return -1; }
int inotify_rm_watch(int fd, int wd) { (void)fd; (void)wd; return -1; }
int eventfd(unsigned int v, int f) { (void)v; (void)f; return -1; }
int eventfd_write(int fd, unsigned long long v) { (void)fd; (void)v; return -1; }
int eventfd_read(int fd, unsigned long long *v) { (void)fd; (void)v; return -1; }
int socket(int d, int t, int p) { (void)d; (void)t; (void)p; return -1; }
int bind(int fd, const void *a, unsigned int l) { (void)fd; (void)a; (void)l; return -1; }
int listen(int fd, int b) { (void)fd; (void)b; return -1; }
int accept(int fd, void *a, unsigned int *l) { (void)fd; (void)a; (void)l; return -1; }
int connect(int fd, const void *a, unsigned int l) { (void)fd; (void)a; (void)l; return -1; }
long recv(int fd, void *b, unsigned long l, int f) { (void)fd; (void)b; (void)l; (void)f; return -1; }
long send(int fd, const void *b, unsigned long l, int f) { (void)fd; (void)b; (void)l; (void)f; return -1; }
int sched_yield(void) { return 0; }
long clock(void) { return 0; }
double difftime(long t1, long t0) { return (double)(t1 - t0); }
long mktime(void *tm) { (void)tm; return 0; }
char *asctime(const void *tm) { (void)tm; return ""; }
char *ctime(const long *t) { (void)t; return ""; }
char *strftime_stub(char *s, unsigned long m, const char *f, const void *t) { (void)s; (void)m; (void)f; (void)t; if(s && m) s[0]=0; return s; }
void *gmtime(const long *t) { (void)t; return NULL; }
void *localtime_simple(const long *t) { (void)t; return NULL; }
unsigned long strftime(char *s, unsigned long m, const char *f, const void *t) { (void)f; (void)t; if(s && m) s[0]=0; return 0; }
int setpriority(int w, int who, int p) { (void)w; (void)who; (void)p; return 0; }
int gettimeofday(struct timeval *tv, void *tz) { (void)tz; if(tv){tv->tv_sec=0;tv->tv_usec=0;} return 0; }
int pthread_getcpuclockid(unsigned long t, int *c) { (void)t; *c = 1; return 0; }
int sched_getcpu(void) { return 0; }
int pthread_sigmask(int h, const void *s, void *o) { (void)h; (void)s; (void)o; return 0; }
int pthread_setname_np(unsigned long t, const char *n) { (void)t; (void)n; return 0; }
int flock(int fd, int op) { (void)fd; (void)op; return 0; }
