#include "sys/socket.h"
#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "syscall.h"

static int copy_unix_path(const struct sockaddr *addr, socklen_t addrlen, char *path_out, size_t path_out_size) {
    const struct sockaddr_un *un = (const struct sockaddr_un *)addr;
    size_t i;

    if (!addr || !path_out || path_out_size == 0 || addrlen < sizeof(sa_family_t)) {
        errno = EINVAL;
        return -1;
    }
    if (un->sun_family != AF_UNIX) {
        errno = ENOTSUP;
        return -1;
    }

    for (i = 0; i + 1 < path_out_size && i < sizeof(un->sun_path); i++) {
        path_out[i] = un->sun_path[i];
        if (un->sun_path[i] == '\0') {
            break;
        }
    }
    path_out[path_out_size - 1] = '\0';
    if (path_out[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int socket(int domain, int type, int protocol) {
    if (domain != AF_UNIX || type != SOCK_STREAM || protocol != 0) {
        errno = ENOTSUP;
        return -1;
    }

    int fd = (int)syscall4(SYS_FS, FS_CMD_UNIX_SOCKET_CREATE, (uint64_t)domain, (uint64_t)type, (uint64_t)protocol);
    if (fd < 0) {
        errno = ENOSYS;
        return -1;
    }
    return fd;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    char path[108];

    if (copy_unix_path(addr, addrlen, path, sizeof(path)) < 0) {
        return -1;
    }

    int rc = (int)syscall4(SYS_FS, FS_CMD_UNIX_SOCKET_CONNECT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen);
    if (rc < 0) {
        errno = ENOENT;
        return -1;
    }
    return rc;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    char path[108];

    if (copy_unix_path(addr, addrlen, path, sizeof(path)) < 0) {
        return -1;
    }

    int rc = (int)syscall4(SYS_FS, FS_CMD_UNIX_SOCKET_BIND, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen);
    if (rc < 0) {
        errno = EINVAL;
        return -1;
    }
    return rc;
}

int listen(int sockfd, int backlog) {
    int rc = (int)syscall3(SYS_FS, FS_CMD_UNIX_SOCKET_LISTEN, (uint64_t)sockfd, (uint64_t)backlog);
    if (rc < 0) {
        errno = ENOTSUP;
        return -1;
    }
    return rc;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    int rc;

    for (;;) {
        rc = (int)syscall4(SYS_FS, FS_CMD_UNIX_SOCKET_ACCEPT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen);
        if (rc != -2) {
            break;
        }
        sys_yield();
    }

    if (rc < 0) {
        errno = EAGAIN;
        return -1;
    }
    return rc;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    (void)flags;
    int rc = sys_write(sockfd, (const char *)buf, (int)len);
    if (rc < 0) {
        errno = EPIPE;
        return -1;
    }
    return rc;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    (void)flags;
    int rc = sys_read(sockfd, buf, (uint32_t)len);
    if (rc < 0) {
        errno = EAGAIN;
        return -1;
    }
    return rc;
}

int shutdown(int sockfd, int how) {
    (void)sockfd;
    (void)how;
    errno = ENOTSUP;
    return -1;
}

void perror(const char *s) {
    const char *msg = strerror(errno);
    if (s && s[0]) {
        printf("%s: %s\n", s, msg ? msg : "unknown error");
    } else {
        printf("%s\n", msg ? msg : "unknown error");
    }
}
