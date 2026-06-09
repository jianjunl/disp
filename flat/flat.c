
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "flat.h"

// 默认配置
static const flat_conf_t default_conf = {
    .malloc = malloc,
    .calloc = calloc,
    .free   = free,
    .init_cap = 16
};

// 内部扩容函数（翻倍策略）
static void flat_array_grow(flat_array_t *arr) {
    size_t new_cap = arr->cap * 2;
    if (new_cap < 8) new_cap = 8;  // 最小容量保护
        // 如果没有 realloc，则手动分配并拷贝
    void *new_data = arr->conf->malloc(new_cap * arr->elem_size);
    if (!new_data) abort();
    memcpy(new_data, arr->data, arr->len * arr->elem_size);
    arr->conf->free(arr->data);
    arr->data = new_data;
    arr->cap = new_cap;
}

flat_array_t* flat_array_create(const flat_conf_t *conf) {
    const flat_conf_t *c = conf ? conf : &default_conf;
    if (c->init_cap == 0) c = &default_conf;

    flat_array_t *arr = c->malloc(sizeof(flat_array_t));
    if (!arr) return NULL;

    arr->elem_size = sizeof(flat_val_t);
    arr->len = 0;
    arr->cap = c->init_cap;
    arr->conf = c;

    arr->data = c->calloc(arr->cap, arr->elem_size);
    if (!arr->data) {
        c->free(arr);
        return NULL;
    }
    return arr;
}

void flat_array_destroy(flat_array_t *arr) {
    if (!arr) return;
    arr->conf->free(arr->data);
    arr->conf->free(arr);
}

void flat_array_push(flat_array_t *arr, flat_val_t val) {
    if (arr->len == arr->cap) {
        flat_array_grow(arr);
    }
    ((flat_val_t*)arr->data)[arr->len++] = val;
}

flat_val_t flat_array_pop(flat_array_t *arr) {
    if (arr->len == 0) return FLAT_NULL;
    flat_val_t val = ((flat_val_t*)arr->data)[arr->len - 1];
    arr->len--;
    // 可选：当 len < cap/4 且 cap > 初始容量时缩容，本例省略
    return val;
}

size_t flat_array_length(const flat_array_t *arr) {
    return arr->len;
}

flat_val_t flat_array_get(const flat_array_t *arr, size_t idx) {
    assert(idx < arr->len);
    return ((flat_val_t*)arr->data)[idx];
}

void flat_array_set(flat_array_t *arr, size_t idx, flat_val_t val) {
    assert(idx < arr->len);
    ((flat_val_t*)arr->data)[idx] = val;
}
