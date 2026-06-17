
#ifndef SKIP_H
#define SKIP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct skip_data {
    uintptr_t k;
    uintptr_t v;
} skip_data;

// 比较函数：返回负数表示 a < b, 0 表示相等, 正数表示 a > b
typedef int (*skip_cmp)(uintptr_t a, uintptr_t b);

// 默认整数比较函数
static inline int skip_default_cmp(uintptr_t a, uintptr_t b) {
    return (a < b) ? -1 : (a > b) ? 1 : 0;
}

// 可定制的内存分配器
typedef struct skip_conf {
    skip_cmp cmp;
    void *(*malloc)(size_t size);
    void *(*calloc)(size_t nmemb, size_t size);
    void (*free)(void *ptr);
} skip_conf;

typedef struct skip_node skip_node;

typedef struct skip_list skip_list;

skip_list *skip_create(skip_conf *conf);
void skip_destroy(skip_list *sl);

skip_node *skip_search(skip_list *sl, uintptr_t key);
bool skip_add(skip_list *sl, skip_data value, bool unique);
bool skip_delete(skip_list *sl, uintptr_t key);

// 遍历接口（基于第 0 层单向链表）
skip_node *skip_first(const skip_list *sl);
skip_node *skip_next(const skip_node *node);

// 安全读取节点值
skip_data skip_node_value(const skip_node *node);

#endif
