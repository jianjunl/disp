#ifndef _CHUNCKED_H
#define _CHUNCKED_H

#include <stddef.h>
#include <stdint.h>

typedef struct chuncked_conf {
    void* (*malloc)(size_t size);
    void* (*calloc)(size_t nmemb, size_t size);
    void  (*free)(void *ptr);
    size_t block_size;                  // 每个数据块容纳的元素个数
} chuncked_conf_t;

typedef struct chuncked_array chuncked_array_t;

// 创建数组，conf 为 NULL 时使用默认配置（malloc/free，block_size=16）
chuncked_array_t* chuncked_array_create(const chuncked_conf_t *conf);

// 销毁数组
void chuncked_array_destroy(chuncked_array_t *arr);

size_t chuncked_array_length(const chuncked_array_t *arr);
uint64_t chuncked_array_get(const chuncked_array_t *arr, size_t idx);
void chuncked_array_put(chuncked_array_t *arr, size_t idx, uint64_t val);
void chuncked_array_add(chuncked_array_t *arr, uint64_t val);

#endif // _CHUNCKED_H
