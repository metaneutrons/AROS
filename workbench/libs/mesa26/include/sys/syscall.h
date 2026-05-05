#ifndef SYS_SYSCALL_H_STUB
#define SYS_SYSCALL_H_STUB
#define SYS_gettid 0
#define SYS_memfd_create 0
static inline long syscall(long n, ...) { (void)n; return -1; }
#endif
