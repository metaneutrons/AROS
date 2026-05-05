#ifndef AROS_MESA_POSIX_H
#define AROS_MESA_POSIX_H
/* Extra POSIX declarations for Mesa on AROS */
#include <stddef.h>
int posix_memalign(void **ptr, size_t align, size_t size);
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int snprintf(char *str, size_t size, const char *format, ...);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
int pipe(int pipefd[2]);
int fcntl(int fd, int cmd, ...);
#endif
