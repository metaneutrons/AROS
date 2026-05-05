#ifndef SYS_EVENTFD_H_STUB
#define SYS_EVENTFD_H_STUB
#define EFD_CLOEXEC 0
static inline int eventfd(unsigned int initval, int flags) { (void)initval; (void)flags; return -1; }
#endif
