#ifndef SYS_MMAN_H_STUB
#define SYS_MMAN_H_STUB
#include <stddef.h>
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_FAILED ((void *)-1)
#define MAP_SHARED 1
void *mmap(void *a, size_t l, int p, int f, int fd, long o);
int munmap(void *a, size_t l);
int mprotect(void *a, size_t l, int p);
#endif
