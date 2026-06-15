
#ifndef SKIP_H
#define SKIP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// 比较函数：返回负数表示 a < b, 0 表示相等, 正数表示 a > b
typedef int (*skip_cmp)(uintptr_t a, uintptr_t b);

// 默认整数比较函数
static inline int default_cmp(uintptr_t a, uintptr_t b) {
    return (a < b) ? -1 : (a > b) ? 1 : 0;
}

// 可定制的内存分配器
typedef struct skip_conf {
    skip_cmp cmp;
    void *(*malloc)(size_t size);
    void *(*calloc)(size_t nmemb, size_t size);
    void (*free)(void *ptr);
} skip_conf;

// 跳表节点（公开，用户可直接读取 value，但不应修改 forward）
typedef struct skip_node {
    uintptr_t value;
    struct skip_node **forward;  // 动态数组，长度等于该节点的层数
} skip_node;

// 跳表结构（不透明，用户不应直接访问内部成员）
typedef struct skip_list skip_list;

// 创建 / 销毁
skip_list *skip_create(skip_conf *conf);
void skip_destroy(skip_list *sl);

// 基本操作
bool skip_search(skip_list *sl, uintptr_t target);
// 可重复键插入
void skip_insert_dup(skip_list *sl, uintptr_t value);
// 实现“不存在则插入，存在则返回” 不使用skip_insert_dup保证“唯一键”
bool skip_insert(skip_list *sl, uintptr_t value);
// 实现“不存在则插入，存在则替换值” 不使用skip_insert_dup保证“唯一键”
bool skip_update(skip_list *sl, uintptr_t value);
bool skip_delete(skip_list *sl, uintptr_t value);

// 遍历接口（基于第 0 层单向链表）
skip_node *skip_first(const skip_list *sl);
skip_node *skip_next(const skip_node *node);

// 安全读取节点值
static inline uintptr_t skip_node_value(const skip_node *node) {
    return node->value;
}

#endif
