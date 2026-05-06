#ifndef SYS_INOTIFY_H_STUB
#define SYS_INOTIFY_H_STUB
#define IN_MODIFY 0x02
#define IN_CREATE 0x100
#define IN_DELETE 0x200
#define IN_CLOSE_WRITE 0x08
#define IN_MOVED_TO 0x80
#define IN_NONBLOCK 0x800
#define IN_CLOEXEC 0x80000
#define IN_MOVE 0x0C0
#define IN_DELETE_SELF 0x400
#define IN_MOVE_SELF 0x800
#define IN_MOVED_FROM 0x40
#define IN_ATTRIB 0x04
#define IN_ALL_EVENTS 0xFFF
#define IN_ONLYDIR 0x1000000
struct inotify_event { int wd; unsigned int mask; unsigned int cookie; unsigned int len; char name[]; };
int inotify_init1(int flags);
int inotify_add_watch(int fd, const char *pathname, unsigned int mask);
int inotify_rm_watch(int fd, int wd);
#endif
