/* Minimal sys/socket.h for BoredOS userland */
#ifndef SYS_SOCKET_H
#define SYS_SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

typedef uint16_t sa_family_t;
typedef uint32_t socklen_t;

#define AF_UNIX 1
#define AF_LOCAL AF_UNIX
#define AF_INET 2
#define AF_INET6 10

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

#define UNIX_PATH_MAX 108
struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[UNIX_PATH_MAX];
};

/* Basic socket prototypes (implemented by libc/syscall glue or lwIP)
 * ssize_t/read/write/close are in other headers.
 */
int socket(int domain, int type, int protocol);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
int shutdown(int sockfd, int how);

#endif
