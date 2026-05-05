#define _SC_PHYS_PAGES 85
#undef __STRICT_ANSI__
#include <unistd.h>
#include <time.h>
#include <aros/types/timespec_s.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

/* POSIX functions Mesa needs that AROS doesn't declare */
int posix_memalign(void **ptr, size_t align, size_t size);
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
int pipe(int pipefd[2]);
int fcntl(int fd, int cmd, ...);
int mkstemp(char *tmpl);
int ftruncate(int fd, long length);
int mkstemps(char *tmpl, int suffixlen);
int ftruncate(int fd, long length);
int sched_yield(void);
int getpwuid_r(unsigned int uid, void *pwd, char *buf, size_t len, void **res);
int clock_nanosleep(int clk, int flags, const void *req, void *rem);
const char *util_get_process_name(void);
long syscall(long number, ...);
int asprintf(char **strp, const char *fmt, ...);

/* clock defines */
#define HAVE_STRUCT_TIMESPEC 1
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define TIMER_ABSTIME 1
#endif

/* stdio additions */
#include <stdio.h>
FILE *fdopen(int fd, const char *mode);
FILE *open_memstream(char **ptr, size_t *sizeloc);

/* epoll stubs */
#ifndef EPOLL_CTL_ADD
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CLOEXEC 0
#define EPOLLIN 1
#define EPOLLOUT 4
struct epoll_event { unsigned int events; union { void *ptr; int fd; } data; };
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
#endif

#ifdef __cplusplus
/* sysconf */
long sysconf(int name);
int setpriority(int which, int who, int prio);
int flock(int fd, int operation);
#define PRIO_PROCESS 0
/* epoll */
#ifndef EPOLL_CTL_ADD
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CLOEXEC 0
#define EPOLLIN 1
#define EPOLLOUT 4
struct epoll_event { unsigned int events; union { void *ptr; int fd; } data; };
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
#endif
}
#endif
