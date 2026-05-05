#ifndef SYSLOG_H_STUB
#define SYSLOG_H_STUB
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_INFO 6
#define LOG_DEBUG 7
#define LOG_NDELAY 0x08
#define LOG_PID 0x01
#define LOG_USER (1<<3)
static inline void openlog(const char *i, int o, int f) { (void)i; (void)o; (void)f; }
static inline void syslog(int p, const char *f, ...) { (void)p; (void)f; }
static inline void closelog(void) {}
#endif
