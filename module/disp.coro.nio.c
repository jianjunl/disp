
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ucontext.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>      /* for fcntl, O_NONBLOCK */
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

/* =============================== 非阻塞文件 I/O =============================== */

// 设置文件描述符为非阻塞模式
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 辅助：从文件对象获取 fd，并确保非阻塞
#pragma GCC diagnostic ignored "-Wunused-function"
static int get_nonblock_fd(disp_val *file_obj) {
    if (T(file_obj) != DISP_FILE) return -1;
    FILE *f = disp_get_file(file_obj);
    if (!f) return -1;
    int fd = fileno(f);
    if (fd == -1) return -1;
    // 设置为非阻塞（只设置一次，可以检查标志避免重复）
    set_nonblocking(fd);
    return fd;
}

static disp_val* fread_nb_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_FILE)
        ERET(NIL, "fread-nb expects a file object");
    FILE *f = disp_get_file(args[0]);
    if (!f) return NIL;
    int fd = fileno(f);
    if (fd == -1) return NIL;
    set_nonblocking(fd);

    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 数据未就绪，挂起协程等待可读
            disp_val *current = disp_get_current_coro();
            event_loop_add_fd(fd, current, EPOLLIN);
            scheduler_suspend();
            // 唤醒后重试
            n = read(fd, buf, sizeof(buf) - 1);
            if (n == -1) {
                if (errno == EAGAIN) return NIL; // 仍然未就绪，不应该发生
                return NIL;
            }
        } else {
            return NIL;
        }
    }
    if (n == 0) return NIL; // EOF
    buf[n] = '\0';
    return disp_make_string(buf);
}

static disp_val* fwrite_nb_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_FILE || T(args[1]) != DISP_STRING)
        ERET(NIL, "fwrite-nb expects (file string)");
    FILE *f = disp_get_file(args[0]);
    if (!f) return NIL;
    int fd = fileno(f);
    if (fd == -1) return NIL;
    set_nonblocking(fd);

    const char *str = disp_get_str(args[1]);
    size_t len = strlen(str);
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, str + written, len - written);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                disp_val *current = disp_get_current_coro();
                event_loop_add_fd(fd, current, EPOLLOUT);
                scheduler_suspend();
                continue;
            } else {
                return NIL;
            }
        }
        written += n;
    }
    return TRUE;
}

/* =============================== go 和 sleep =============================== */

static disp_val* go_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_CLOSURE)
        ERET(NIL, "go expects a lambda");
    disp_val *coro = disp_make_coroutine(args[0], 65536);
    scheduler_add(coro);
    return coro;
}

static disp_val* sleep_ms_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "sleep-ms expects milliseconds");
    long ms = 0;
    if (T(args[0]) == DISP_INT)
        ms = disp_get_int(args[0]);
    else if (T(args[0]) == DISP_LONG)
        ms = disp_get_long(args[0]);
    else
        ERET(NIL, "sleep-ms: argument must be integer");
    event_loop_add_timer(ms, disp_get_current_coro());
    scheduler_suspend();
    return NIL;
}

/* =============================== 模块初始化 =============================== */

void disp_init_module(void) {
    DEF("go-func"       , MKF(go_syscall             , "<go-func>"       ), 1);
    DEF("sleep-ms"      , MKF(sleep_ms_syscall       , "<sleep-ms>"      ), 1);
    DEF("fread-nb"      , MKF(fread_nb_syscall       , "<fread-nb>"      ), 1);
    DEF("fwrite-nb"     , MKF(fwrite_nb_syscall      , "<fwrite-nb>"     ), 1);
}
