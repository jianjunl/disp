
#ifndef CORO_H
#define CORO_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef DEBUG
//#define DEBUG
#endif

/* --- Coroutine --- */
disp_box disp_get_current_coro();
disp_box disp_make_coroutine(disp_box func, size_t stack_size);
void scheduler_add(disp_box coro);
void scheduler_yield(void);
void scheduler_suspend(void);
void event_loop_add_fd(int fd, disp_box coro, int events);
void event_loop_add_timer(long milliseconds, disp_box coro);
disp_box disp_make_socket(int fd);
int disp_get_socket_fd(disp_box v);

#endif // CORO_H
