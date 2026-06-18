#ifndef FLAT_ARRAY_H
#define FLAT_ARRAY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint64_t flat_val_t;
#define FLAT_NULL 0ULL

// 分配器配置
typedef struct flat_conf {
    void* (*malloc)(size_t size);
    void* (*calloc)(size_t nmemb, size_t size);
    void  (*free)(void *ptr);
    size_t init_cap;      // 初始容量（元素个数），0 表示使用默认
} flat_conf_t;

// 动态数组结构
typedef struct flat_array {
    void *data;           // 连续内存
    size_t len;           // 实际元素个数
    size_t cap;           // 容量（元素个数）
    size_t elem_size;     // 元素大小（固定 sizeof(flat_val_t)）
    const flat_conf_t *conf;  // 配置（含分配器）
} flat_array_t;

// 创建数组（conf 为 NULL 时使用默认配置）
flat_array_t* flat_array_create(const flat_conf_t *conf);

// 销毁数组
void flat_array_destroy(flat_array_t *arr);

// 尾部追加元素
void flat_array_push(flat_array_t *arr, flat_val_t val);

// 尾部弹出元素（若数组为空返回 FLAT_NULL）
flat_val_t flat_array_pop(flat_array_t *arr);

// 获取元素个数
size_t flat_array_length(const flat_array_t *arr);

// 获取元素（不检查边界，调用者保证 idx < len）
flat_val_t flat_array_get(const flat_array_t *arr, size_t idx);

// 设置元素（不检查边界）
void flat_array_set(flat_array_t *arr, size_t idx, flat_val_t val);

void* flat_array_get_ptr(const flat_array_t *arr, size_t idx);

#endif // FLAT_ARRAY_H
