
#ifndef DISP_CORO_C
#ifndef DISP_CORO_CHAN_C
#ifndef DISP_CORO_POLL_C
#error "internal use for disp.coro(.(chan|poll)).c"
#endif
#endif
#endif

#ifndef DISP_CORO_H
#define DISP_CORO_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <ucontext.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

typedef struct disp_coro_t {
    ucontext_t ctx;
    int status;            // 0=就绪, 1=运行中, 2=结束
    disp_val *func;        // 协程入口函数（闭包）
    disp_val *yield_value; // yield 返回给 resume 的值
    disp_val *resume_arg;  // resume 传入给 yield 的参数
    disp_val *final_result;// 协程最终返回值
    int timer_fd;          // 关联的 timerfd（-1 表示无）
    // select 扩展
    void *select_data;     // 用于 select 的临时数据（保留）
    void *stack;           // 独立栈指针，用于释放
    disp_val *next;
} disp_coro_t;

// 通道对象结构（通过 disp_val 的 union 存储）
typedef struct disp_channel_t {
    int cap;
    int size;
    int head;
    int tail;
    disp_val **buffer;
    disp_val *wait_send;      // 等待发送的协程队列
    disp_val *wait_recv;      // 等待接收的协程队列
    gc_mutex_t *lock;
    int closed;
    disp_val *direct_value;   // 用于无缓冲通道的临时值传递
} disp_channel_t;

#ifndef DISP_ALLINONE_UNION_H
union disp_data {
    /* 协程 */
    disp_coro_t *coro;

    /* 通道 */
    disp_channel_t *chan;
};
#endif

#endif /* DISP_CORO_H */
