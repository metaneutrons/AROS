#ifndef SYS_SOCKET_H_STUB
#define SYS_SOCKET_H_STUB
#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOCK_CLOEXEC 0x80000
struct sockaddr { unsigned short sa_family; char sa_data[14]; };
struct sockaddr_un { unsigned short sun_family; char sun_path[108]; };
int socket(int domain, int type, int protocol);
int bind(int fd, const struct sockaddr *addr, unsigned int len);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr *addr, unsigned int *len);
int connect(int fd, const struct sockaddr *addr, unsigned int len);
long recv(int fd, void *buf, unsigned long len, int flags);
long send(int fd, const void *buf, unsigned long len, int flags);
#endif
