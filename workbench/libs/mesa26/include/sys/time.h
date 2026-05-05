#ifndef SYS_TIME_H_STUB
#define SYS_TIME_H_STUB
#include <time.h>
struct timeval { long tv_sec; long tv_usec; };
static inline int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (tv) { tv->tv_sec = 0; tv->tv_usec = 0; }
    return 0;
}
#endif
