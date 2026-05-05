#ifdef __cplusplus
extern "C" {
#endif

#ifndef AROS_MESA_POSIX_H
#include <sys/types.h>
#define AROS_MESA_POSIX_H
/* Extra POSIX declarations for Mesa on AROS */
#include <stddef.h>
int posix_memalign(void **ptr, size_t align, size_t size);
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
int pipe(int pipefd[2]);
int fcntl(int fd, int cmd, ...);
#endif

/* File I/O stubs */
int mkstemp(char *tmpl);
int ftruncate(int fd, long length);
char *realpath(const char *path, char *resolved);
char *strdup(const char *s);
int dup(int fd);

/* Additional file ops */
int mkstemps(char *tmpl, int suffixlen);
int symlink(const char *target, const char *linkpath);
int readlink(const char *path, char *buf, size_t bufsiz);

/* Disable disk cache at compile time */
struct passwd;
int getpwuid_r(unsigned int uid, struct passwd *pwd, char *buf, size_t len, struct passwd **res);
#define ENABLE_SHADER_CACHE 0

/* stdio additions */
#include <stdio.h>
FILE *fdopen(int fd, const char *mode);
int clock_nanosleep(int clk, int flags, const void *req, void *rem);
FILE *open_memstream(char **ptr, size_t *sizeloc);

/* Mesa internal */
const char *util_get_process_name(void);
int asprintf(char **strp, const char *fmt, ...);
int vasprintf(char **strp, const char *fmt, __builtin_va_list ap);
#ifdef __cplusplus
int asprintf(char **strp, const char *fmt, ...);
}
#endif
