
#ifndef __MODULE_THREAD_C
#error "internal use for disp.thread.c"
#endif

#ifndef __MODULE_THREAD_H
#define __MODULE_THREAD_H

#include <pthread.h>
#include "../disp.h"

/* 线程对象结构 */
typedef struct disp_thread_t {
    pthread_t tid;
    disp_val func;          // 要执行的闭包
    disp_val result;        // 线程返回值
    int finished;            // 是否已结束
    gc_mutex_t *lock;        // 保护 result/finished 的互斥锁
    gc_cond_t  *cond;        // 用于 join 的条件变量
} disp_thread_t;

#if DISP_NAN_BOXING
struct disp_data {
    disp_flag_t tag;
union {
#else // DISP_NAN_BOXING
union disp_data {
#endif // DISP_NAN_BOXING
    /* 线程 */
    struct disp_thread_t *thread;
    /* 互斥锁 */
    gc_mutex_t *mutex;
    /* 条件变量 */
    gc_cond_t *cond;
#if DISP_NAN_BOXING
};
#endif // DISP_NAN_BOXING
};

#if DISP_NAN_BOXING
GC_STRUCT_TI(disp_data,
#else // DISP_NAN_BOXING
GC_UNION_TI(disp_data,
#endif // DISP_NAN_BOXING
    GC_OFF(disp_data, thread),
    GC_OFF(disp_data, mutex),
    GC_OFF(disp_data, cond)
);

#endif /* __MODULE_THREAD_H */
