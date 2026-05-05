#ifndef SCHED_H_STUB
#define SCHED_H_STUB
#define SCHED_OTHER 0
typedef struct { unsigned long bits[1]; } cpu_set_t;
static inline int sched_setaffinity(int p, size_t s, const cpu_set_t *c) { (void)p; (void)s; (void)c; return 0; }
static inline int sched_yield(void) { return 0; }
#endif
