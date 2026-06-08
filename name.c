
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "array.h"
#include "bt/btree.h"

static inline int bt_cmp(bt_key_t a, bt_key_t b) {
    return strcmp((char *)a, (char *)b);
}

static bt_conf_t bt_conf_nogc = (bt_conf_t) {
    .malloc = malloc,
    .calloc = calloc,
    .free   = free,
    .cmp    = bt_cmp,
    .t      = 3 // 最小度数（每个节点至少有 t-1 个键）
};

static btree_t *g_name_to_id      = NULL;   // 存储字符串指针 (uint64_t 强转为 char*)
static disp_array_t *g_id_to_name = NULL;   // 存储字符串指针
static gc_mutex_t *g_name_lock;

// 初始化全局名称表
void disp_init_name_table(void) {
    g_name_to_id = btree_create(&bt_conf_nogc);
    g_id_to_name = disp_array_create(256);      // 块大小 256
    gc_pthread_mutex_init(&g_name_lock, NULL);
    gc_add_root(&g_name_lock);
    // 加入一个占位 ID 0（无效 ID）
    disp_array_add(g_id_to_name, (uint64_t)NULL);
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
    uint64_t new_id = disp_array_length(g_id_to_name);
    char *name_copy = strdup(name);
    disp_array_add(g_id_to_name, (uint64_t)name_copy);
    btree_insert(g_name_to_id, (bt_key_t)name_copy, new_id);
    gc_pthread_mutex_unlock(g_name_lock);
    return new_id;
}

// 根据 ID 获取名称（用于调试或打印）
const char* disp_get_name(uint64_t id) {
    if (id == 0 || id >= disp_array_length(g_id_to_name))
        return NULL;
    return (const char*)disp_array_get(g_id_to_name, id);
}
