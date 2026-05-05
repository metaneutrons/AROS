#ifndef FTW_H_STUB
#define FTW_H_STUB
#define FTW_F 1
#define FTW_D 2
struct FTW { int base; int level; };
typedef int (*__ftw_func_t)(const char *, const void *, int);
static inline int nftw(const char *p, __ftw_func_t f, int d, int fl)
    { (void)p; (void)f; (void)d; (void)fl; return -1; }
#endif
