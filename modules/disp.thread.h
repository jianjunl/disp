
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

struct disp_data {
#if DISP_NAN_BOXING
    disp_flag_t tag;
#endif // DISP_NAN_BOXING
    union {
        /* 线程 */
        struct disp_thread_t *thread;
        /* 互斥锁 */
        gc_mutex_t *mutex;
        /* 条件变量 */
        gc_cond_t *cond;
    };
};

GC_STRUCT_TI(disp_data,
    GC_OFF(disp_data, thread),
    GC_OFF(disp_data, mutex),
    GC_OFF(disp_data, cond)
);

#endif /* __MODULE_THREAD_H */
