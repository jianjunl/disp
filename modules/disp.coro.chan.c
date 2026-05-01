
#ifndef DISP_CORO_CHAN_C
#define DISP_CORO_CHAN_C

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
#include <fcntl.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"
#include "disp.coro.h"

disp_channel_t* disp_get_channel(disp_val *v) {
    return v->data->chan;
}

void disp_set_channel(disp_val *v, disp_channel_t *c) {
    GC_WRITE_BARRIER(v, c);
    v->data->chan = c;
}

/* =============================== 通道（支持无缓冲握手） =============================== */

static disp_val* make_channel_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_INT)
        ERET(NIL, "make-chan expects capacity (integer)");
    int cap = disp_get_int(args[0]);
    if (cap < 0) cap = 0;
    disp_val *v = DISP_ALLOC(DISP_CHAN);
    disp_channel_t *ch = gc_calloc(1, sizeof(struct disp_channel_t));
    ch->cap = cap;
    ch->size = 0;
    ch->head = 0;
    ch->tail = 0;
    ch->buffer = (cap > 0) ? gc_calloc(cap, sizeof(disp_val*)) : NULL;
    ch->lock = gc_calloc(1, sizeof(pthread_mutex_t));
    ch->wait_send = NULL;
    ch->wait_recv = NULL;
    ch->closed = 0;
    ch->direct_value = NIL;
    gc_pthread_mutex_init(&ch->lock, NULL);
    disp_set_channel(v, ch);
    return v;
}

static disp_val* channel_send_syscall(disp_val **args, int count) {
    if (count != 2) ERET(NIL, "send expects (channel value)");
    if (T(args[0]) != DISP_CHAN) ERET(NIL, "first argument must be a channel");
    disp_channel_t *ch = disp_get_channel(args[0]);
    disp_val *value = args[1];
    while (1) {
        gc_pthread_mutex_lock(ch->lock);
        if (ch->closed) {
            gc_pthread_mutex_unlock(ch->lock);
            ERET(NIL, "send on closed channel");
        }

        if (ch->cap == 0) {
            // 无缓冲通道：直接与等待的接收者握手
            if (ch->wait_recv) {
                disp_val *waiter = ch->wait_recv;
                ch->wait_recv = waiter->data->coro->next;
                // 将值传递给接收者（存入 direct_value）
                ch->direct_value = value;
                scheduler_add(waiter);
                gc_pthread_mutex_unlock(ch->lock);
                return value;
            }
            // 没有等待的接收者，挂起发送者
            disp_val *current = disp_get_current_coro();
            current->data->coro->next = ch->wait_send;
            ch->wait_send = current;
            gc_pthread_mutex_unlock(ch->lock);
            scheduler_suspend();
            // 被唤醒后，说明有接收者取走了值，直接返回
            return value;
        } else {
            // 有缓冲通道
            if (ch->size < ch->cap) {
                ch->buffer[ch->tail] = value;
                ch->tail = (ch->tail + 1) % ch->cap;
                ch->size++;
                if (ch->wait_recv) {
                    disp_val *waiter = ch->wait_recv;
                    ch->wait_recv = waiter->data->coro->next;
                    scheduler_add(waiter);
                }
                gc_pthread_mutex_unlock(ch->lock);
                return value;
            } else {
                disp_val *current = disp_get_current_coro();
                current->data->coro->next = ch->wait_send;
                ch->wait_send = current;
                gc_pthread_mutex_unlock(ch->lock);
                scheduler_suspend();
            }
        }
    }
}

static disp_val* channel_recv_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "recv expects a channel");
    if (T(args[0]) != DISP_CHAN) ERET(NIL, "argument must be a channel");
    disp_channel_t *ch = disp_get_channel(args[0]);
    while (1) {
        gc_pthread_mutex_lock(ch->lock);
        if (ch->closed && ch->size == 0 && ch->wait_send == NULL) {
            gc_pthread_mutex_unlock(ch->lock);
            return NIL;
        }

        if (ch->cap == 0) {
            // 无缓冲通道：直接与等待的发送者握手
            if (ch->wait_send) {
                disp_val *waiter = ch->wait_send;
                ch->wait_send = waiter->data->coro->next;
                disp_val *value = ch->direct_value;
                ch->direct_value = NIL;
                scheduler_add(waiter);
                gc_pthread_mutex_unlock(ch->lock);
                return value;
            }
            // 没有等待的发送者，挂起接收者
            disp_val *current = disp_get_current_coro();
            current->data->coro->next = ch->wait_recv;
            ch->wait_recv = current;
            gc_pthread_mutex_unlock(ch->lock);
            scheduler_suspend();
            // 被唤醒后，从 direct_value 中取值
            gc_pthread_mutex_lock(ch->lock);
            disp_val *value = ch->direct_value;
            ch->direct_value = NIL;
            gc_pthread_mutex_unlock(ch->lock);
            return value;
        } else {
            // 有缓冲通道
            if (ch->size > 0) {
                disp_val *value = ch->buffer[ch->head];
                ch->head = (ch->head + 1) % ch->cap;
                ch->size--;
                if (ch->wait_send) {
                    disp_val *waiter = ch->wait_send;
                    ch->wait_send = waiter->data->coro->next;
                    scheduler_add(waiter);
                }
                gc_pthread_mutex_unlock(ch->lock);
                return value;
            } else if (ch->closed) {
                gc_pthread_mutex_unlock(ch->lock);
                return NIL;
            } else {
                disp_val *current = disp_get_current_coro();
                current->data->coro->next = ch->wait_recv;
                ch->wait_recv = current;
                gc_pthread_mutex_unlock(ch->lock);
                scheduler_suspend();
            }
        }
    }
}

// recv2 类似，为简洁略作修改（支持无缓冲）
static disp_val* channel_recv2_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "recv2 expects a channel");
    if (T(args[0]) != DISP_CHAN) ERET(NIL, "argument must be a channel");
    disp_channel_t *ch = disp_get_channel(args[0]);
    while (1) {
        gc_pthread_mutex_lock(ch->lock);
        if (ch->closed && ch->size == 0 && ch->wait_send == NULL) {
            gc_pthread_mutex_unlock(ch->lock);
            return disp_make_cons(NIL, TRUE);
        }

        if (ch->cap == 0) {
            if (ch->wait_send) {
                disp_val *waiter = ch->wait_send;
                ch->wait_send = waiter->data->coro->next;
                disp_val *value = ch->direct_value;
                ch->direct_value = NIL;
                scheduler_add(waiter);
                gc_pthread_mutex_unlock(ch->lock);
                return disp_make_cons(value, NIL);
            }
            disp_val *current = disp_get_current_coro();
            current->data->coro->next = ch->wait_recv;
            ch->wait_recv = current;
            gc_pthread_mutex_unlock(ch->lock);
            scheduler_suspend();
            gc_pthread_mutex_lock(ch->lock);
            disp_val *value = ch->direct_value;
            ch->direct_value = NIL;
            gc_pthread_mutex_unlock(ch->lock);
            return disp_make_cons(value, NIL);
        } else {
            if (ch->size > 0) {
                disp_val *value = ch->buffer[ch->head];
                ch->head = (ch->head + 1) % ch->cap;
                ch->size--;
                if (ch->wait_send) {
                    disp_val *waiter = ch->wait_send;
                    ch->wait_send = waiter->data->coro->next;
                    scheduler_add(waiter);
                }
                gc_pthread_mutex_unlock(ch->lock);
                return disp_make_cons(value, NIL);
            } else if (ch->closed) {
                gc_pthread_mutex_unlock(ch->lock);
                return disp_make_cons(NIL, TRUE);
            } else {
                disp_val *current = disp_get_current_coro();
                current->data->coro->next = ch->wait_recv;
                ch->wait_recv = current;
                gc_pthread_mutex_unlock(ch->lock);
                scheduler_suspend();
            }
        }
    }
}

static disp_val* close_channel_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_CHAN)
        ERET(NIL, "close-channel expects a channel");
    disp_channel_t *ch = disp_get_channel(args[0]);
    gc_pthread_mutex_lock(ch->lock);
    if (ch->closed) {
        gc_pthread_mutex_unlock(ch->lock);
        ERET(NIL, "close-channel: channel already closed");
    }
    ch->closed = 1;
    // 唤醒所有等待接收的协程
    while (ch->wait_recv) {
        disp_val *waiter = ch->wait_recv;
        ch->wait_recv = waiter->data->coro->next;
        scheduler_add(waiter);
    }
    // 唤醒所有等待发送的协程（它们会检测到 closed 并报错）
    while (ch->wait_send) {
        disp_val *waiter = ch->wait_send;
        ch->wait_send = waiter->data->coro->next;
        scheduler_add(waiter);
    }
    gc_pthread_mutex_unlock(ch->lock);
    return TRUE;
}

void disp_init_module(void) {
    DEF("make-channel"  , MKF(make_channel_syscall   , "<make-chan>"     ), 1);
    DEF("send"          , MKF(channel_send_syscall   , "<send>"          ), 1);
    DEF("recv"          , MKF(channel_recv_syscall   , "<recv>"          ), 1);
    DEF("recv2"         , MKF(channel_recv2_syscall  , "<recv2>"         ), 1);
    DEF("close-channel" , MKF(close_channel_syscall  , "<close-channel>" ), 1);
}

#endif /*  DISP_CORO_CHAN_C */
