#ifndef SYS_STAT_H_STUB
#define SYS_STAT_H_STUB
#include_next <sys/stat.h>
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & 0170000) == 0040000)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & 0170000) == 0100000)
#endif
static inline int mkdir_stub(const char *p, int m) { (void)p; (void)m; return -1; }
#define mkdir(p, m) mkdir_stub(p, m)
#endif
