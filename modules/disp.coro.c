
#ifndef DISP_CORO_C
#define DISP_CORO_C

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
#include "disp.coro.h"

/* =============================== 协程核心 =============================== */

static ucontext_t main_ctx;      // 调度器（事件循环）的主上下文
static disp_val *current_coro = NULL;

// 全局就绪队列
static disp_val *ready_head = NULL;
static disp_val *ready_tail = NULL;

static void coroutine_entry(disp_val *coro) {
    gc_add_root(&coro);
    current_coro = coro;
    coro->data->coro->status = 1;
    disp_val *func = coro->data->coro->func;
    disp_val *result = disp_apply_closure(func, NULL, 0);
    coro->data->coro->final_result = result;
    coro->data->coro->status = 2;
    current_coro = NULL;
    swapcontext(&coro->data->coro->ctx, &main_ctx);
    gc_remove_root(&coro);
}

disp_val* disp_make_coroutine(disp_val *func, size_t stack_size) {
    disp_val *v = DISP_ALLOC(DISP_CORO);
    disp_coro_t *c = gc_calloc(1, sizeof(struct disp_coro_t));
    v->data->coro = c;
    c->status = 0;
    c->func = func;
    c->yield_value = NIL;
    c->resume_arg = NIL;
    c->final_result = NIL;
    c->timer_fd = -1;
    c->select_data = NULL;
    void *s = gc_malloc(stack_size);
    v->data->coro->stack = s;
    if (!s) {
        gc_free(v); gc_free(c);
        return NIL;
    }
    getcontext(&c->ctx);
    c->ctx.uc_stack.ss_sp = s;
    c->ctx.uc_stack.ss_size = stack_size;
    c->ctx.uc_link = NULL;
    makecontext(&c->ctx, (void(*)(void))coroutine_entry, 1, v);
    return v;
}

static disp_val* make_coroutine_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_CLOSURE)
        ERET(NIL, "make-coroutine expects a lambda");
    return disp_make_coroutine(args[0], 65536);
}

static disp_val* yield_syscall(disp_val **args, int count) {
    if (current_coro == NULL)
        ERET(NIL, "yield called outside coroutine");
    disp_coro_t *c = current_coro->data->coro;
    c->yield_value = (count >= 1) ? args[0] : NIL;
    // 主动让出：放回就绪队列
    if (c->status == 1) {
        if (ready_head == NULL) {
            ready_head = ready_tail = current_coro;
        } else {
            ready_tail->data->coro->next = current_coro;
            ready_tail = current_coro;
        }
        current_coro->data->coro->next = NULL;
    }
    current_coro = NULL;
    swapcontext(&c->ctx, &main_ctx);
    // 恢复后，获取 resume 传入的参数
    disp_val *arg = c->resume_arg;
    c->resume_arg = NIL;
    return arg;
}

static disp_val* resume_syscall(disp_val **args, int count) {
    if (count < 1 || T(args[0]) != DISP_CORO)
        ERET(NIL, "resume expects a coroutine and optional argument");
    disp_val *coro = args[0];
    disp_coro_t *c =coro->data->coro;
    if (c->status == 2)
        return c->final_result ? c->final_result : NIL;
    if (current_coro != NULL)
        ERET(NIL, "resume: already inside a coroutine (call yield first)");
    c->resume_arg = (count >= 2) ? args[1] : NIL;
    current_coro = coro;
    c->status = 1;
    swapcontext(&main_ctx, &c->ctx);
    if (c->status == 2)
        return c->final_result ? c->final_result : NIL;
    else {
        disp_val *result = c->yield_value;
        c->yield_value = NIL;
        return result;
    }
}

disp_val* disp_get_current_coro() {
    return current_coro ? current_coro : NIL;
}

static disp_val* current_coro_syscall(disp_val **args, int count) {
    (void)args; (void)count;
    return current_coro ? current_coro : NIL;
}

void scheduler_add(disp_val *coro) {
    if (!coro || T(coro) != DISP_CORO) {
        INFO("scheduler_add: invalid coro %p (flag=%d)", (void*)coro, coro != NULL ? T(coro) : DISP_VOID);
        exit(1);
    }
    if (ready_head == NULL) {
        ready_head = ready_tail = coro;
    } else {
        ready_tail->data->coro->next = coro;
        ready_tail = coro;
    }
    coro->data->coro->next = NULL;
}

void scheduler_yield(void) {
    if (current_coro == NULL) return;
    disp_coro_t *c = current_coro->data->coro;
    if (c->status == 1) {
        scheduler_add(current_coro);
    }
    current_coro = NULL;
    swapcontext(&c->ctx, &main_ctx);
}

void scheduler_suspend(void) {
    if (current_coro == NULL) return;
    disp_coro_t *c = current_coro->data->coro;
    c->status = 0;          // 标记为挂起
    current_coro = NULL;
    swapcontext(&c->ctx, &main_ctx);
}


/* =============================== 事件循环 =============================== */

#define MAX_EVENTS 64
static int epfd = -1;
static struct epoll_event events[MAX_EVENTS];

void event_loop_init(void) {
    if (epfd == -1) {
        epfd = epoll_create1(0);
        if (epfd == -1) perror("epoll_create1");
    }
}

void event_loop_add_fd(int fd, disp_val *coro, int events) {
    if (epfd == -1) event_loop_init();
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = coro;
    AAA(coro, data, coro);
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        if (errno == EEXIST) {
            // The fd is already there, so modify it instead
            epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
        } else {
            // Handle other actual errors (e.g., ENOMEM, EBADF)
            perror("epoll_ctl ADD");
        }
    }
    coro->data->coro->status = 0;   // 挂起
}

void event_loop_add_timer(long milliseconds, disp_val *coro) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tfd == -1) {
        perror("timerfd_create");
        return;
    }
    // 记录 timerfd 到协程对象
    disp_coro_t *c = coro->data->coro;
    c->timer_fd = tfd;

    struct itimerspec its;
    its.it_value.tv_sec = milliseconds / 1000;
    its.it_value.tv_nsec = (milliseconds % 1000) * 1000000;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    if (timerfd_settime(tfd, 0, &its, NULL) == -1) {
        perror("timerfd_settime");
        close(tfd);
        c->timer_fd = -1;
        return;
    }
    event_loop_add_fd(tfd, coro, EPOLLIN);
}

void event_loop_run(void) {
    if (epfd == -1) event_loop_init();
    getcontext(&main_ctx);          // 初始化主上下文
    while (1) {
        // 先运行就绪队列中的协程
        if (ready_head) {
            disp_val *next = ready_head;
            if (T(next) != DISP_CORO) {
                INFO("event_loop_run: non-coroutine in ready queue! flag=%d\n", T(next));
                exit(1);
            }
            ready_head = ready_head->data->coro->next;
            if (ready_head == NULL) ready_tail = NULL;
            if (next != current_coro) {
                disp_coro_t *c = next->data->coro;
                current_coro = next;
                c->status = 1;
                swapcontext(&main_ctx, &c->ctx);
            }
            continue;
        }
        // 没有就绪协程，等待 epoll 事件
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            disp_val *coro = events[i].data.ptr;
            disp_coro_t *c = coro->data->coro;
            // 关闭 timerfd（如果有）
            if (c->timer_fd != -1) {
                close(c->timer_fd);
                c->timer_fd = -1;
            }
            scheduler_add(coro);
            coro->data->coro->status = 1;
        }
    }
}

static disp_val* event_loop_run_syscall(disp_val **args, int count) {
    (void)args; (void)count;
    event_loop_run();
    return TRUE; // never reached
}

/* =============================== 模块初始化 =============================== */

void disp_init_module(void) {
    gc_add_root(&current_coro);
    gc_add_root(&ready_head);
    gc_add_root(&ready_tail);
    DEF("make-coroutine", MKF(make_coroutine_syscall, "<make-coroutine>"), 1);
    DEF("yield"         , MKF(yield_syscall          , "<yield>"         ), 1);
    DEF("resume"        , MKF(resume_syscall         , "<resume>"        ), 1);
    DEF("current-coro"  , MKF(current_coro_syscall   , "<current-coro>"  ), 1);
    DEF("event-loop"    , MKF(event_loop_run_syscall , "<event-loop>"    ), 1);
    disp_load("disp.coro.chan.so");
    disp_load("disp.coro.poll.so");
    disp_load("disp.coro.nio.so");
    disp_load("disp.coro.net.so");
} 

#endif /* DISP_CORO_C */
