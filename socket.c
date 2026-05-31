
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

struct disp_data {
    /* 套接字 */
#if DISP_NAN_BOXING
    disp_flag_t tag;
#endif // DISP_NAN_BOXING
    int fd;
};

disp_val disp_make_socket(int fd) {
    disp_val v = ALLOC(FLAG_EXTRA, TAG_SOCKET);
    D(v)->fd = fd;
    return v;
}

int disp_get_socket_fd(disp_val v) {
    if (T(v) != TAG_SOCKET) return -1;
    return D(v)->fd;
}
