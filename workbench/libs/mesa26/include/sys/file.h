#ifndef SYS_FILE_H_STUB
#define SYS_FILE_H_STUB
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_UN 8
#define LOCK_NB 4
int flock(int fd, int operation);
#endif
