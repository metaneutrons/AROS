#ifndef SYS_EPOLL_H_STUB
#define SYS_EPOLL_H_STUB
#define EPOLL_CLOEXEC 0
#define EPOLLIN 1
#define EPOLLOUT 4
struct epoll_event { unsigned int events; union { void *ptr; int fd; } data; };
static inline int epoll_create1(int f) { (void)f; return -1; }
static inline int epoll_ctl(int e, int op, int fd, struct epoll_event *ev) { (void)e; (void)op; (void)fd; (void)ev; return -1; }
static inline int epoll_wait(int e, struct epoll_event *ev, int max, int t) { (void)e; (void)ev; (void)max; (void)t; return 0; }
#endif
