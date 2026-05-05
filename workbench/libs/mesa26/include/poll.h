#ifndef POLL_H_STUB
#define POLL_H_STUB
#define POLLIN 1
#define POLLOUT 4
struct pollfd { int fd; short events; short revents; };
static inline int poll(struct pollfd *fds, int n, int t) { (void)fds; (void)n; (void)t; return 0; }
#endif
