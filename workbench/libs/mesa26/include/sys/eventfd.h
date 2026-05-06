#ifndef SYS_EVENTFD_H_STUB
#define SYS_EVENTFD_H_STUB
#define EFD_CLOEXEC 0
#define EFD_NONBLOCK 0x800
typedef unsigned long long eventfd_t;
int eventfd(unsigned int initval, int flags) { (void)initval; (void)flags; return -1; }
int eventfd_write(int fd, unsigned long long value);
int eventfd_read(int fd, eventfd_t *value);
#endif
