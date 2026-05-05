#ifndef SYS_AUXV_H_STUB
#define SYS_AUXV_H_STUB
#define AT_HWCAP 16
#define HWCAP_CRC32 (1 << 7)
#define HWCAP_SHA2 (1 << 6)
#define HWCAP_AES (1 << 3)
static inline unsigned long getauxval(unsigned long type) { (void)type; return 0; }
#endif
