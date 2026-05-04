
#ifndef DISP_THREAD_C
#define DISP_THREAD_C

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"
#include "disp.thread.h"


/* ======================== 辅助函数 ======================== */

/* 线程入口函数 */
static void* thread_entry(void *arg) {
    disp_val *thread_obj = (disp_val*)arg;
    disp_thread_t *t = thread_obj->data->thread;
    disp_val *func = t->func;
    
    /* 执行闭包，不传参数 */
    disp_val *result = disp_apply_closure(func, NULL, 0);
    
    /* 保存结果并通知等待者 */
    gc_pthread_mutex_lock(t->lock);
    t->result = result;
    t->finished = 1;
    gc_pthread_cond_broadcast(t->cond);
    gc_pthread_mutex_unlock(t->lock);
    
    return NULL;
}

/* ======================== 系统调用 ======================== */

/* (make-thread func) -> thread */
static disp_val* make_thread_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_CLOSURE)
        ERET(NIL, "make-thread expects a closure");
    
    disp_val *v = DISP_ALLOC(DISP_THREAD);
    disp_thread_t *t = gc_calloc(1, sizeof(disp_thread_t));
    if (!t) {
        gc_free(v->data); gc_free(v);
        return NIL;
    }
    v->data->thread = t;
    t->func = args[0];
    t->finished = 0;
    t->result = NIL;
    static gc_mutex_t lock = GC_PTHREAD_MUTEX_INITIALIZER;
    t->lock = &lock;
    
    if (gc_pthread_mutex_init(&t->lock, NULL) != 0) {
        gc_free(t); gc_free(v->data); gc_free(v);
        ERET(NIL, "make-thread: mutex init failed");
    }
    if (gc_pthread_cond_init(&t->cond, NULL) != 0) {
        gc_pthread_mutex_destroy(t->lock);
        gc_free(t); gc_free(v->data); gc_free(v);
        ERET(NIL, "make-thread: cond init failed");
    }
    
    if (gc_pthread_create(&t->tid, NULL, thread_entry, v) != 0) {
        gc_pthread_cond_destroy(t->cond);
        gc_pthread_mutex_destroy(t->lock);
        gc_free(t); gc_free(v->data); gc_free(v);
        ERET(NIL, "make-thread: gc_pthread_create failed");
    }
    
    return v;
}

/* (thread-join thread) -> result */
static disp_val* thread_join_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_THREAD)
        ERET(NIL, "thread-join expects a thread object");
    
    disp_val *thread_obj = args[0];
    disp_thread_t *t = thread_obj->data->thread;
    
    gc_pthread_mutex_lock(t->lock);
    while (!t->finished) {
        gc_pthread_cond_wait(t->cond, t->lock);
    }
    disp_val *result = t->result;
    gc_pthread_mutex_unlock(t->lock);
    
    /* 可选：分离线程以回收资源（已结束，可安全 join）*/
    gc_pthread_join(t->tid, NULL);
    
    return result;
}

/* (make-mutex) -> mutex */
static disp_val* make_mutex_syscall(disp_val **args, int count) {
    (void)args; (void)count;
    
    disp_val *v = DISP_ALLOC(DISP_MUTEX);
    if (gc_pthread_mutex_init(&v->data->mutex, NULL) != 0) {
        gc_free(v->data); gc_free(v);
        ERET(NIL, "make-mutex: init failed");
    }
    return v;
}

/* (lock mutex) -> true */
static disp_val* lock_mutex_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_MUTEX)
        ERET(NIL, "lock expects a mutex");
    gc_mutex_t *m = args[0]->data->mutex;
    if (gc_pthread_mutex_lock(m) != 0)
        ERET(NIL, "lock: failed");
    return TRUE;
}

/* (unlock mutex) -> true */
static disp_val* unlock_mutex_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_MUTEX)
        ERET(NIL, "unlock expects a mutex");
    gc_mutex_t *m = args[0]->data->mutex;
    if (gc_pthread_mutex_unlock(m) != 0)
        ERET(NIL, "unlock: failed");
    return TRUE;
}

/* (make-condition) -> cond */
static disp_val* make_condition_syscall(disp_val **args, int count) {
    (void)args; (void)count;
    
    disp_val *v = DISP_ALLOC(DISP_COND);
    if (gc_pthread_cond_init(&v->data->cond, NULL) != 0) {
        gc_free(v->data); gc_free(v);
        ERET(NIL, "make-condition: init failed");
    }
    return v;
}

/* (condition-wait cond mutex) -> true */
static disp_val* condition_wait_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_COND || T(args[1]) != DISP_MUTEX)
        ERET(NIL, "condition-wait expects (cond mutex)");
    gc_cond_t *c = args[0]->data->cond;
    gc_mutex_t *m = args[1]->data->mutex;
    if (gc_pthread_cond_wait(c, m) != 0)
        ERET(NIL, "condition-wait: failed");
    return TRUE;
}

/* (condition-signal cond) -> true */
static disp_val* condition_signal_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_COND)
        ERET(NIL, "condition-signal expects a cond");
    gc_cond_t *c = args[0]->data->cond;
    if (gc_pthread_cond_signal(c) != 0)
        ERET(NIL, "condition-signal: failed");
    return TRUE;
}

/* (condition-broadcast cond) -> true */
static disp_val* condition_broadcast_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_COND)
        ERET(NIL, "condition-broadcast expects a cond");
    gc_cond_t *c = args[0]->data->cond;
    if (gc_pthread_cond_broadcast(c) != 0)
        ERET(NIL, "condition-broadcast: failed");
    return TRUE;
}

/* (thread-sleep seconds) -> nil */
static disp_val* thread_sleep_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "thread-sleep expects seconds");
    double secs = 0.0;
    if (T(args[0]) == DISP_BYTE)
        secs = disp_get_byte(args[0]);
    else if (T(args[0]) == DISP_SHORT)
        secs = disp_get_short(args[0]);
    else if (T(args[0]) == DISP_INT)
        secs = disp_get_int(args[0]);
    else if (T(args[0]) == DISP_LONG)
        secs = disp_get_long(args[0]);
    else if (T(args[0]) == DISP_FLOAT)
        secs = disp_get_float(args[0]);
    else if (T(args[0]) == DISP_DOUBLE)
        secs = disp_get_double(args[0]);
    else
        ERET(NIL, "thread-sleep: numeric argument expected");
    
    struct timespec req, rem;
    req.tv_sec = (time_t)secs;
    req.tv_nsec = (secs - req.tv_sec) * 1000000000.0;

    /* 循环处理信号中断 */
    while (nanosleep(&req, &rem) == -1) {
        if (errno != EINTR) {
            /* 真正的错误 */
            ERET(NIL, "thread-sleep: nanosleep failed");
        }
        /* 被信号中断，用剩余时间继续睡眠 */
        req = rem;
    }
    return NIL;
}

/* ======================== 模块初始化 ======================== */

void disp_init_module(void) {
    DEF("make-thread",        MKF(make_thread_syscall,        "<make-thread>"),        1);
    DEF("thread-join",        MKF(thread_join_syscall,        "<thread-join>"),        1);
    DEF("make-mutex",         MKF(make_mutex_syscall,         "<make-mutex>"),         1);
    DEF("lock",               MKF(lock_mutex_syscall,         "<lock>"),               1);
    DEF("unlock",             MKF(unlock_mutex_syscall,       "<unlock>"),             1);
    DEF("make-condition",     MKF(make_condition_syscall,     "<make-condition>"),     1);
    DEF("condition-wait",     MKF(condition_wait_syscall,     "<condition-wait>"),     1);
    DEF("condition-signal",   MKF(condition_signal_syscall,   "<condition-signal>"),   1);
    DEF("condition-broadcast",MKF(condition_broadcast_syscall,"<condition-broadcast>"),1);
    DEF("thread-sleep",       MKF(thread_sleep_syscall,       "<thread-sleep>"),       1);
}

#endif /* DISP_THREAD_C */
