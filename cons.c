
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
#include "array.h"   // 引入分块数组，用于管理池中的大块内存

union disp_data {
    struct {
        disp_val *car;
        disp_val *cdr;
    } cons;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, cons.car),
    GC_OFF(disp_data, cons.cdr)
);

static gc_mutex_t *g_cons_pool_lock;

// ========== cons data 池（bump allocator）==========
// 每个池块是一个大块内存，包含多个连续的 union disp_data
#define CONS_DATA_BLOCK_SIZE   (4096 * 16)            // 每个块的总字节数
#define DATA_PER_BLOCK         (CONS_DATA_BLOCK_SIZE / sizeof(union disp_data))

static disp_array_t *g_data_blocks = NULL;    // 存储每个分配的大块指针（GC 根）
static char *g_current_ptr = NULL;            // 当前块中可用的起始地址
static char *g_current_end = NULL;            // 当前块的结束地址
static size_t g_blocks_allocated = 0;         // 已分配的块数（可选，仅用于统计）

// 初始化 cons 池（在 disp_init 中调用）
void disp_init_cons_pool(void) {
    if (g_data_blocks) return;
    gc_pthread_mutex_init(&g_cons_pool_lock, NULL);
    // 创建一个由 GC 管理的数组，存储每个大块指针（元素类型为指针数组）
    g_data_blocks = disp_array_create_gc(16, &GC_TYPE_PTR_ARRAY);
    gc_add_root(&g_data_blocks);
    gc_add_root(&g_cons_pool_lock);
}

// 从池中分配一个 union disp_data 结构
static union disp_data* alloc_cons_data(void) {
    gc_pthread_mutex_lock(g_cons_pool_lock);
    if (!g_current_ptr || g_current_ptr + sizeof(union disp_data) > g_current_end) {
        // 当前块已满或尚未分配，分配新块
        union disp_data *new_block = gc_typed_calloc(1, CONS_DATA_BLOCK_SIZE, &union_disp_data_ti);
        if (!new_block) abort();
        // 将块指针存入数组（防止 GC 回收）
        disp_array_add(g_data_blocks, (uint64_t)(uintptr_t)new_block);
        g_current_ptr = (char*)new_block;
        g_current_end = (char*)new_block + CONS_DATA_BLOCK_SIZE;
        ++g_blocks_allocated;
    }
    union disp_data *result = (union disp_data*)g_current_ptr;
    g_current_ptr += sizeof(union disp_data);
    gc_pthread_mutex_unlock(g_cons_pool_lock);
    return result;
}

disp_val* disp_make_cons(disp_val *car, disp_val *cdr) {
    // 从池中获得 data 内存（零开销），头部仍使用常规分配（也可池化，但一次分配影响不大）
    union disp_data *data = alloc_cons_data();
    disp_val *v = disp_alloc(DISP_CONS, data);
    v->data->cons.car = car;
    v->data->cons.cdr = cdr;
    return v;
}

disp_val* disp_car(disp_val *cons) {
    return (cons && cons->flag == DISP_CONS) ? cons->data->cons.car : NIL;
}

disp_val* disp_cdr(disp_val *cons) {
    return (cons && cons->flag == DISP_CONS) ? cons->data->cons.cdr : NIL;
}

void disp_set_car(disp_val *cons, disp_val *car) {
    if (cons && cons->flag == DISP_CONS) GC_ASSIGN_PTR(cons->data->cons.car, car);
}

void disp_set_cdr(disp_val *cons, disp_val *cdr) {
    if (cons && cons->flag == DISP_CONS) GC_ASSIGN_PTR(cons->data->cons.cdr, cdr);
}
