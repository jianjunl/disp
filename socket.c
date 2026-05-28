
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <ucontext.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

#if DISP_NAN_BOXING
struct disp_data {
    disp_flag_t tag;
    /* 套接字 */
    struct {
        int fd;
    } socket_val;
};

#else // DISP_NAN_BOXING

union disp_data {
    /* 套接字 */
    struct {
        int fd;
    } socket_val;
};

#endif // DISP_NAN_BOXING

disp_val disp_make_socket(int fd) {
    disp_val v = ALLOC(FLAG_EXTRA, TAG_SOCKET);
    D(v)->socket_val.fd = fd;
    return v;
}

int disp_get_socket_fd(disp_val v) {
    if (T(v) != TAG_SOCKET) return -1;
    return D(v)->socket_val.fd;
}
