#ifndef UNISTD_H_MESA_STUB
#define UNISTD_H_MESA_STUB
#include_next <unistd.h>
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 84
#endif
#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE 30
#endif
static inline int getpid_stub(void) { return 1; }
static inline int getuid_stub(void) { return 0; }
static inline int access_stub(const char *p, int m) { (void)p; (void)m; return -1; }
static inline int unlink_stub(const char *p) { (void)p; return -1; }
static inline int close_stub(int fd) { (void)fd; return 0; }
static inline long sysconf_stub(int n) { if (n == _SC_NPROCESSORS_ONLN) return 4; return 4096; }
#ifndef getpid
#define getpid getpid_stub
#endif
#ifndef getuid
#define getuid getuid_stub
#endif
#ifndef sysconf
#define sysconf sysconf_stub
#endif
#endif
