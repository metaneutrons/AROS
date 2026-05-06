#ifndef SYS_SYSINFO_H_STUB
#define SYS_SYSINFO_H_STUB
struct sysinfo { unsigned long totalram; unsigned long freeram; };
static inline int sysinfo(struct sysinfo *i) { i->totalram = 1024*1024*1024; i->freeram = 512*1024*1024; return 0; }
#endif
