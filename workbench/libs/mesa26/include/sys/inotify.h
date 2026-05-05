#ifndef SYS_INOTIFY_H_STUB
#define SYS_INOTIFY_H_STUB
#define IN_MODIFY 2
#define IN_CLOEXEC 0
static inline int inotify_init1(int f) { (void)f; return -1; }
static inline int inotify_add_watch(int fd, const char *p, unsigned int m) { (void)fd; (void)p; (void)m; return -1; }
#endif
