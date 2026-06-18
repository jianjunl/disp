
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "gc/gc.h"
#include "flat/flat.h"

//#define DISP_NAME_BTREE 1

#ifndef DISP_NAME_BTREE

#ifndef DISP_NAME_SKIP

#include "gc/robin/robin_table.h"

static flat_conf_t flat_conf = {
    .malloc = malloc,
    .calloc = calloc,
    .free   = free,
    .init_cap = 256   // 初始容量 256
};

static flat_array_t  *g_id_to_name = NULL;
static robin_table_t *g_name_to_id = NULL;   // 存储字符串指针 (uint64_t 强转为 char*)
static gc_mutex_t    *g_name_lock;

// 初始化全局名称表
void disp_init_name_table(void) {
    g_name_to_id = robin_table_create(1024, robin_table_rapidhash, RT_RAPID_SEED, NULL);
    g_id_to_name = flat_array_create(&flat_conf);
    gc_pthread_mutex_init(&g_name_lock, NULL);
    gc_add_root(&g_name_lock);
    // 加入一个占位 ID 0（无效 ID）
    flat_array_push(g_id_to_name, (flat_val_t)NULL);  // 占位 ID 0
}

// 获取或分配 ID（线程安全）
uint64_t disp_get_id(const char *name) {
    if (!name) return 0;
    gc_pthread_mutex_lock(g_name_lock);

    void* v = robin_table_get(g_name_to_id, name, strlen(name));
    if (v) {
        gc_pthread_mutex_unlock(g_name_lock);
        return (uint64_t)v;
    }
    
    // 未找到，分配新 ID
    uint64_t new_id = flat_array_length(g_id_to_name);
    char *name_copy = strdup(name);
    flat_array_push(g_id_to_name, (flat_val_t)name_copy);
    robin_table_put(g_name_to_id, name_copy, strlen(name_copy), (void*)new_id);
    gc_pthread_mutex_unlock(g_name_lock);
    return new_id;
}

// 根据 ID 获取名称（用于调试或打印）
const char* disp_get_name(uint64_t id) {
    if (id == 0 || id >= flat_array_length(g_id_to_name))
        return NULL;
    return (const char*)flat_array_get(g_id_to_name, id);
}

void* disp_get_id_ptr(uint64_t id) {
    if (id == 0 || id >= flat_array_length(g_id_to_name))
        return NULL;
    return flat_array_get_ptr(g_id_to_name, id);
}

#else // DISP_NAME_SKIP

#include "skip/skip.h"

static inline int str_cmp(uintptr_t a, uintptr_t b) {
    return strcmp((char *)a, (char *)b);
}

static skip_conf skip_str_conf = {
    .cmp = str_cmp,
    .malloc = malloc,
    .calloc = calloc,
    .free = free
};

static flat_conf_t flat_conf = {
    .malloc = malloc,
    .calloc = calloc,
    .free   = free,
    .init_cap = 256   // 初始容量 256
};

static flat_array_t *g_id_to_name = NULL;
static skip_list *g_name_to_id    = NULL;   // 存储字符串指针 (uint64_t 强转为 char*)
static gc_mutex_t *g_name_lock;

// 初始化全局名称表
void disp_init_name_table(void) {
    g_name_to_id = skip_create(&skip_str_conf);;
    g_id_to_name = flat_array_create(&flat_conf);
    gc_pthread_mutex_init(&g_name_lock, NULL);
    gc_add_root(&g_name_lock);
    // 加入一个占位 ID 0（无效 ID）
    flat_array_push(g_id_to_name, (flat_val_t)NULL);  // 占位 ID 0
}

// 获取或分配 ID（线程安全）
uint64_t disp_get_id(const char *name) {
    if (!name) return 0;
    gc_pthread_mutex_lock(g_name_lock);

    skip_node *n = skip_search(g_name_to_id, (uintptr_t)name);
    if (n) {
        gc_pthread_mutex_unlock(g_name_lock);
        return skip_node_value(n).v;
    }
    
    // 未找到，分配新 ID
    uint64_t new_id = flat_array_length(g_id_to_name);
    char *name_copy = strdup(name);
    flat_array_push(g_id_to_name, (flat_val_t)name_copy);
    skip_add(g_name_to_id, (skip_data){(uintptr_t)name_copy, (uintptr_t)new_id}, true);
    gc_pthread_mutex_unlock(g_name_lock);
    return new_id;
}

// 根据 ID 获取名称（用于调试或打印）
const char* disp_get_name(uint64_t id) {
    if (id == 0 || id >= flat_array_length(g_id_to_name))
        return NULL;
    return (const char*)flat_array_get(g_id_to_name, id);
}

void* disp_get_id_ptr(uint64_t id) {
    if (id == 0 || id >= flat_array_length(g_id_to_name))
        return NULL;
    return flat_array_get_ptr(g_id_to_name, id);
}

#endif // DISP_NAME_SKIP

#else // DISP_NAME_BTREE

#include "btree/btree.h"

static inline int bt_cmp(bt_key_t a, bt_key_t b) {
    return strcmp((char *)a, (char *)b);
}

static bt_conf_t bt_conf_nogc = {
    .malloc = malloc,
    .calloc = calloc,
    .free   = free,
    .cmp    = bt_cmp,
    .tier   = 3 // 最小度数（每个节点至少有 t-1 个键）
};

static flat_conf_t flat_conf = {
    .malloc = malloc,
    .calloc = calloc,
    .free   = free,
    .init_cap = 256   // 初始容量 256
};

static flat_array_t *g_id_to_name = NULL;
static btree_t *g_name_to_id      = NULL;   // 存储字符串指针 (uint64_t 强转为 char*)
static gc_mutex_t *g_name_lock;

// 初始化全局名称表
void disp_init_name_table(void) {
    g_name_to_id = btree_create(&bt_conf_nogc);
    g_id_to_name = flat_array_create(&flat_conf);
    gc_pthread_mutex_init(&g_name_lock, NULL);
    gc_add_root(&g_name_lock);
    // 加入一个占位 ID 0（无效 ID）
    flat_array_push(g_id_to_name, (flat_val_t)NULL);  // 占位 ID 0
}

// 获取或分配 ID（线程安全）
uint64_t disp_get_id(const char *name) {
    if (!name) return 0;
    gc_pthread_mutex_lock(g_name_lock);

    uint64_t i = btree_search(g_name_to_id, (bt_key_t)name);
    if (i) {
        gc_pthread_mutex_unlock(g_name_lock);
        return i;
    }
    
    // 未找到，分配新 ID
    uint64_t new_id = flat_array_length(g_id_to_name);
    char *name_copy = strdup(name);
    flat_array_push(g_id_to_name, (flat_val_t)name_copy);
    btree_insert(g_name_to_id, (bt_key_t)name_copy, new_id);
    gc_pthread_mutex_unlock(g_name_lock);
    return new_id;
}

// 根据 ID 获取名称（用于调试或打印）
const char* disp_get_name(uint64_t id) {
    if (id == 0 || id >= flat_array_length(g_id_to_name))
        return NULL;
    return (const char*)flat_array_get(g_id_to_name, id);
}

void* disp_get_id_ptr(uint64_t id) {
    if (id == 0 || id >= flat_array_length(g_id_to_name))
        return NULL;
    return flat_array_get_ptr(g_id_to_name, id);
}

#endif // DISP_NAME_BTREE
