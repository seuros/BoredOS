/* Minimal sys/un.h for Unix domain socket constants */
#ifndef SYS_UN_H
#define SYS_UN_H

#include <sys/socket.h>

#define SOCKADDR_UN_MAX_PATH 108

/* nothing else needed beyond sockaddr_un in sys/socket.h */

#endif
