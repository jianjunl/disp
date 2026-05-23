
#ifndef DISP_ARRAY_H
#define DISP_ARRAY_H

#include <stddef.h>
#include <stdint.h>
#include "disp.h"

typedef struct disp_array disp_array_t;

// 默认创建（使用 malloc/calloc/free）
disp_array_t* disp_array_create(size_t block_size);

// 创建由 GC 管理的数组（根块用 &GC_TYPE_PTR_ARRAY，数据块用传入的 elem_type_info）
disp_array_t* disp_array_create_gc(size_t block_size, const gc_type_info_t *elem_type_info);

// 销毁数组
void disp_array_destroy(disp_array_t *arr);

size_t disp_array_length(const disp_array_t *arr);
uint64_t disp_array_get(const disp_array_t *arr, size_t idx);
void disp_array_put(disp_array_t *arr, size_t idx, uint64_t val);
void disp_array_add(disp_array_t *arr, uint64_t val);

#endif // DISP_ARRAY_H
