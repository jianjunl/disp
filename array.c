
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "array.h"

// ---------- 分配器抽象 ----------
typedef struct {
    void* (*alloc_root)(size_t size);
    void* (*alloc_block)(size_t count, size_t elem_size, const gc_type_info_t *type_info);
    void  (*free)(void *ptr);
} disp_allocator_t;

// ---------- 默认分配器 (malloc/free) ----------
static void* default_alloc_root(size_t size) {
    return malloc(size);
}
static void* default_alloc_block(size_t count, size_t elem_size, const gc_type_info_t *type_info) {
    (void)type_info;
    return calloc(count, elem_size);
}
static void default_free(void *ptr) {
    free(ptr);
}
static const disp_allocator_t default_allocator = {
    .alloc_root = default_alloc_root,
    .alloc_block = default_alloc_block,
    .free = default_free
};

// ---------- GC 分配器 ----------
static void* gc_alloc_root(size_t size) {
    return gc_typed_malloc(size, &GC_TYPE_PTR_ARRAY);
}
static void* gc_alloc_block(size_t count, size_t elem_size, const gc_type_info_t *type_info) {
    return gc_typed_calloc(count, elem_size, type_info);
}
static void gc_typed_free(void *ptr) {
    gc_free(ptr);
}
static const disp_allocator_t gc_allocator = {
    .alloc_root = gc_alloc_root,
    .alloc_block = gc_alloc_block,
    .free = gc_typed_free
};

// ---------- 数组结构体 ----------
struct disp_array {
    size_t block_size;                 // 每个数据块元素个数
    size_t elem_size;                  // 元素字节数（固定 sizeof(uint64_t)）
    const gc_type_info_t *elem_type;   // 元素类型信息（GC 版本有效，malloc 版本为 NULL）
    const disp_allocator_t *alloc;     // 分配器函数表
    size_t num_blocks;                 // 已分配的数据块数
    size_t blocks_cap;                 // 块指针数组容量
    size_t length;                     // 总元素个数
    uint64_t **blocks;                 // 指向每个数据块的指针
};

// ---------- 内部函数：扩展块指针数组（不使用 realloc，兼容 GC 分配器）----------
static void ensure_blocks_capacity(disp_array_t *arr) {
    if (arr->num_blocks < arr->blocks_cap) return;
    size_t new_cap = arr->blocks_cap * 2;
    // 使用分配器分配新数组
    uint64_t **new_blocks = arr->alloc->alloc_root(sizeof(uint64_t*) * new_cap);
    if (!new_blocks) abort();
    // 复制旧内容
    memcpy(new_blocks, arr->blocks, sizeof(uint64_t*) * arr->num_blocks);
    // 释放旧数组（使用分配器的 free）
    arr->alloc->free(arr->blocks);
    arr->blocks = new_blocks;
    arr->blocks_cap = new_cap;
}

// ---------- 公开函数 ----------
disp_array_t* disp_array_create(size_t block_size) {
    if (block_size == 0) block_size = 16;
    disp_array_t *arr = default_allocator.alloc_root(sizeof(disp_array_t));
    if (!arr) return NULL;
    *arr = (disp_array_t){
        .block_size = block_size,
        .elem_size = sizeof(uint64_t),
        .elem_type = NULL,
        .alloc = &default_allocator,
        .num_blocks = 0,
        .blocks_cap = 1,
        .length = 0,
        .blocks = NULL
    };
    arr->blocks = default_allocator.alloc_root(sizeof(uint64_t*) * arr->blocks_cap);
    if (!arr->blocks) {
        default_allocator.free(arr);
        return NULL;
    }
    void *first_block = default_allocator.alloc_block(block_size, sizeof(uint64_t), NULL);
    if (!first_block) {
        default_allocator.free(arr->blocks);
        default_allocator.free(arr);
        return NULL;
    }
    arr->blocks[0] = (uint64_t*)first_block;
    arr->num_blocks = 1;
    return arr;
}

disp_array_t* disp_array_create_gc(size_t block_size, const gc_type_info_t *elem_type_info) {
    if (block_size == 0) block_size = 16;
    if (!elem_type_info) return NULL;
    disp_array_t *arr = gc_allocator.alloc_root(sizeof(disp_array_t));
    if (!arr) return NULL;
    *arr = (disp_array_t){
        .block_size = block_size,
        .elem_size = sizeof(uint64_t),
        .elem_type = elem_type_info,
        .alloc = &gc_allocator,
        .num_blocks = 0,
        .blocks_cap = 1,
        .length = 0,
        .blocks = NULL
    };
    arr->blocks = gc_allocator.alloc_root(sizeof(uint64_t*) * arr->blocks_cap);
    if (!arr->blocks) {
        // GC 版本无需显式释放，由 GC 自动回收
        return NULL;
    }
    void *first_block = gc_allocator.alloc_block(block_size, sizeof(uint64_t), elem_type_info);
    if (!first_block) {
        return NULL;
    }
    arr->blocks[0] = (uint64_t*)first_block;
    arr->num_blocks = 1;
    return arr;
}

void disp_array_destroy(disp_array_t *arr) {
    if (!arr) return;
    // 释放所有数据块
    for (size_t i = 0; i < arr->num_blocks; ++i) {
        arr->alloc->free(arr->blocks[i]);
    }
    // 释放块指针数组
    arr->alloc->free(arr->blocks);
    // 释放根块
    arr->alloc->free(arr);
}

size_t disp_array_length(const disp_array_t *arr) {
    return arr->length;
}

uint64_t disp_array_get(const disp_array_t *arr, size_t idx) {
    assert(idx < arr->length);
    size_t block_idx = idx / arr->block_size;
    size_t offset    = idx % arr->block_size;
    return arr->blocks[block_idx][offset];
}

void disp_array_put(disp_array_t *arr, size_t idx, uint64_t val) {
    assert(idx < arr->length);
    size_t block_idx = idx / arr->block_size;
    size_t offset    = idx % arr->block_size;
    arr->blocks[block_idx][offset] = val;
}

void disp_array_add(disp_array_t *arr, uint64_t val) {
    size_t new_len = arr->length + 1;
    if (new_len > arr->num_blocks * arr->block_size) {
        // 分配新数据块
        ensure_blocks_capacity(arr);
        void *new_block = arr->alloc->alloc_block(arr->block_size, arr->elem_size, arr->elem_type);
        if (!new_block) abort();
        arr->blocks[arr->num_blocks] = (uint64_t*)new_block;
        arr->num_blocks++;
    }
    size_t block_idx = arr->length / arr->block_size;
    size_t offset    = arr->length % arr->block_size;
    arr->blocks[block_idx][offset] = val;
    arr->length = new_len;
}
