#ifndef SEMAPHORE_H_STUB
#define SEMAPHORE_H_STUB
typedef int sem_t;
static inline int sem_init(sem_t *s, int p, unsigned v) { (void)s; (void)p; *s = v; return 0; }
static inline int sem_destroy(sem_t *s) { (void)s; return 0; }
static inline int sem_wait(sem_t *s) { (void)s; return 0; }
static inline int sem_post(sem_t *s) { (void)s; return 0; }
#endif
