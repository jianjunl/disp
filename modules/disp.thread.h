
#ifndef DISP_THREAD_C
#error "internal use for disp.thread.c"
#endif

#ifndef DISP_THREAD_H
#define DISP_THREAD_H

#include <pthread.h>
#include "../disp.h"

/* 线程对象结构 */
typedef struct disp_thread_t {
    pthread_t tid;
    disp_val *func;          // 要执行的闭包
    disp_val *result;        // 线程返回值
    int finished;            // 是否已结束
    gc_mutex_t *lock;        // 保护 result/finished 的互斥锁
    gc_cond_t  *cond;        // 用于 join 的条件变量
} disp_thread_t;

#ifndef DISP_ALLINONE_UNION_H
union disp_data {
    /* 线程 */
    struct disp_thread_t *thread;
    /* 互斥锁 */
    gc_mutex_t *mutex;
    /* 条件变量 */
    gc_cond_t *cond;
};
#endif

#endif /* DISP_THREAD_H */
