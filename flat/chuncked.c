
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "chuncked.h"

// 默认配置
static const chuncked_conf_t default_conf = {
    .malloc = malloc,
    .calloc = calloc,
    .free   = free,
    .block_size = 16
};

struct chuncked_array {
    const chuncked_conf_t *conf;        // 配置（含分配器、块大小等）
    size_t elem_size;                   // 元素字节数（固定 sizeof(uint64_t)）
    size_t num_blocks;                  // 已分配的数据块数
    size_t blocks_cap;                  // 块指针数组容量
    size_t length;                      // 总元素个数
    uint64_t **blocks;                  // 指向每个数据块的指针
};

// 内部函数：扩展块指针数组（不使用 realloc，兼容自定义分配器）
static void ensure_blocks_capacity(chuncked_array_t *arr) {
    if (arr->num_blocks < arr->blocks_cap) return;
    size_t new_cap = arr->blocks_cap * 2;
    uint64_t **new_blocks = arr->conf->malloc(sizeof(uint64_t*) * new_cap);
    if (!new_blocks) abort();
    memcpy(new_blocks, arr->blocks, sizeof(uint64_t*) * arr->num_blocks);
    arr->conf->free(arr->blocks);
    arr->blocks = new_blocks;
    arr->blocks_cap = new_cap;
}

chuncked_array_t* chuncked_array_create(const chuncked_conf_t *conf) {
    // 使用默认配置或用户配置
    const chuncked_conf_t *c = conf ? conf : &default_conf;
    if (c->block_size == 0) c = &default_conf;  // 无效块大小，回退默认

    chuncked_array_t *arr = c->malloc(sizeof(chuncked_array_t));
    if (!arr) return NULL;

    *arr = (chuncked_array_t){
        .conf = c,
        .elem_size = sizeof(uint64_t),
        .num_blocks = 0,
        .blocks_cap = 1,
        .length = 0,
        .blocks = NULL
    };

    arr->blocks = c->malloc(sizeof(uint64_t*) * arr->blocks_cap);
    if (!arr->blocks) {
        c->free(arr);
        return NULL;
    }

    // 分配第一个数据块
    void *first_block = c->calloc(c->block_size, sizeof(uint64_t));
    if (!first_block) {
        c->free(arr->blocks);
        c->free(arr);
        return NULL;
    }
    arr->blocks[0] = (uint64_t*)first_block;
    arr->num_blocks = 1;
    return arr;
}

void chuncked_array_destroy(chuncked_array_t *arr) {
    if (!arr) return;
    for (size_t i = 0; i < arr->num_blocks; ++i) {
        arr->conf->free(arr->blocks[i]);
    }
    arr->conf->free(arr->blocks);
    arr->conf->free(arr);
}

size_t chuncked_array_length(const chuncked_array_t *arr) {
    return arr->length;
}

uint64_t chuncked_array_get(const chuncked_array_t *arr, size_t idx) {
    assert(idx < arr->length);
    size_t block_idx = idx / arr->conf->block_size;
    size_t offset    = idx % arr->conf->block_size;
    return arr->blocks[block_idx][offset];
}

void chuncked_array_put(chuncked_array_t *arr, size_t idx, uint64_t val) {
    assert(idx < arr->length);
    size_t block_idx = idx / arr->conf->block_size;
    size_t offset    = idx % arr->conf->block_size;
    arr->blocks[block_idx][offset] = val;
}

void chuncked_array_add(chuncked_array_t *arr, uint64_t val) {
    size_t new_len = arr->length + 1;
    size_t block_size = arr->conf->block_size;
    if (new_len > arr->num_blocks * block_size) {
        ensure_blocks_capacity(arr);
        void *new_block = arr->conf->calloc(block_size, arr->elem_size);
        if (!new_block) abort();
        arr->blocks[arr->num_blocks] = (uint64_t*)new_block;
        arr->num_blocks++;
    }
    size_t block_idx = arr->length / block_size;
    size_t offset    = arr->length % block_size;
    arr->blocks[block_idx][offset] = val;
    arr->length = new_len;
}
