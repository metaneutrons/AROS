#ifndef PTHREAD_H_MESA_STUB
#define PTHREAD_H_MESA_STUB

/* Complete pthread stub for Mesa on AROS (single-threaded) */
#include <stddef.h>
#include <stdint.h>

typedef int pthread_mutex_t;
typedef int pthread_mutexattr_t;
typedef int pthread_rwlock_t;
typedef int pthread_rwlockattr_t;
typedef int pthread_once_t;
typedef unsigned int pthread_key_t;
typedef unsigned long pthread_t;
typedef int pthread_cond_t;
typedef int pthread_condattr_t;
typedef int pthread_attr_t;
typedef int pthread_barrier_t;
typedef int pthread_barrierattr_t;

#define PTHREAD_MUTEX_INITIALIZER 0
#define PTHREAD_RWLOCK_INITIALIZER 0
#define PTHREAD_ONCE_INIT 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_CREATE_DETACHED 1

static inline int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) { (void)m; (void)a; return 0; }
static inline int pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutex_lock(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutex_trylock(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutexattr_init(pthread_mutexattr_t *a) { (void)a; return 0; }
static inline int pthread_mutexattr_settype(pthread_mutexattr_t *a, int t) { (void)a; (void)t; return 0; }
static inline int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void)a; return 0; }

static inline int pthread_rwlock_init(pthread_rwlock_t *l, const pthread_rwlockattr_t *a) { (void)l; (void)a; return 0; }
static inline int pthread_rwlock_destroy(pthread_rwlock_t *l) { (void)l; return 0; }
static inline int pthread_rwlock_rdlock(pthread_rwlock_t *l) { (void)l; return 0; }
static inline int pthread_rwlock_wrlock(pthread_rwlock_t *l) { (void)l; return 0; }
static inline int pthread_rwlock_unlock(pthread_rwlock_t *l) { (void)l; return 0; }

static inline int pthread_once(pthread_once_t *o, void (*f)(void)) { if (!*o) { f(); *o = 1; } return 0; }

static inline int pthread_key_create(pthread_key_t *k, void (*d)(void *)) { static unsigned int n = 0; *k = n++; (void)d; return 0; }
static inline int pthread_key_delete(pthread_key_t k) { (void)k; return 0; }
static inline void *pthread_getspecific(pthread_key_t k) { (void)k; return NULL; }
static inline int pthread_setspecific(pthread_key_t k, const void *v) { (void)k; (void)v; return 0; }

static inline pthread_t pthread_self(void) { return 1; }
static inline int pthread_equal(pthread_t a, pthread_t b) { return a == b; }
static inline int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void*), void *arg) { (void)t; (void)a; (void)f; (void)arg; return -1; }
static inline int pthread_join(pthread_t t, void **r) { (void)t; (void)r; return -1; }
static inline int pthread_detach(pthread_t t) { (void)t; return 0; }

static inline int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) { (void)c; (void)a; return 0; }
static inline int pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }
static inline int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { (void)c; (void)m; return 0; }
static inline int pthread_cond_signal(pthread_cond_t *c) { (void)c; return 0; }
static inline int pthread_cond_broadcast(pthread_cond_t *c) { (void)c; return 0; }
static inline int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const void *t) { (void)c; (void)m; (void)t; return 0; }
static inline int pthread_condattr_init(pthread_condattr_t *a) { (void)a; return 0; }
static inline int pthread_condattr_setclock(pthread_condattr_t *a, int c) { (void)a; (void)c; return 0; }
static inline int pthread_condattr_destroy(pthread_condattr_t *a) { (void)a; return 0; }

static inline int pthread_attr_init(pthread_attr_t *a) { (void)a; return 0; }
static inline int pthread_attr_destroy(pthread_attr_t *a) { (void)a; return 0; }
static inline int pthread_attr_setdetachstate(pthread_attr_t *a, int s) { (void)a; (void)s; return 0; }

static inline int pthread_barrier_init(pthread_barrier_t *b, const pthread_barrierattr_t *a, unsigned c) { (void)b; (void)a; (void)c; return 0; }
static inline int pthread_barrier_destroy(pthread_barrier_t *b) { (void)b; return 0; }
static inline int pthread_barrier_wait(pthread_barrier_t *b) { (void)b; return 0; }

#endif
