#ifndef XF86DRM_H
#define XF86DRM_H
#include <stdint.h>
static inline int drmIoctl(int fd, unsigned long req, void *arg) { (void)fd; (void)req; (void)arg; return -1; }
static inline int drmSyncobjCreate(int fd, uint32_t f, uint32_t *h) { *h=1; (void)fd; (void)f; return 0; }
static inline int drmSyncobjDestroy(int fd, uint32_t h) { (void)fd; (void)h; return 0; }
static inline int drmSyncobjWait(int fd, uint32_t *h, uint32_t c, int64_t t, uint32_t f, uint32_t *r) { (void)fd; (void)h; (void)c; (void)t; (void)f; (void)r; return 0; }
static inline int drmPrimeHandleToFD(int fd, uint32_t h, uint32_t f, int *pfd) { (void)fd; (void)h; (void)f; *pfd=-1; return -1; }
static inline int drmPrimeFDToHandle(int fd, int pfd, uint32_t *h) { (void)fd; (void)pfd; *h=0; return -1; }
static inline int drmSyncobjExportSyncFile(int fd, uint32_t h, int *sync_fd) { (void)fd; (void)h; *sync_fd=-1; return -1; }
static inline int drmSyncobjImportSyncFile(int fd, uint32_t h, int sync_fd) { (void)fd; (void)h; (void)sync_fd; return -1; }
#endif
