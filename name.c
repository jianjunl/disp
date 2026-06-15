
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
//#include "chuncked.h"
#include "gc/gc.h"
#include "flat/flat.h"
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

/*
static chuncked_conf_t chuncked_conf = {
    .malloc = malloc,
    .calloc = calloc,
    .free   = free,
    .block_size = 256
};

static chuncked_array_t *g_id_to_name = NULL;   // 存储字符串指针
*/
static flat_array_t *g_id_to_name = NULL;
static btree_t *g_name_to_id      = NULL;   // 存储字符串指针 (uint64_t 强转为 char*)
static gc_mutex_t *g_name_lock;

// 初始化全局名称表
void disp_init_name_table(void) {
    g_name_to_id = btree_create(&bt_conf_nogc);
    g_id_to_name = flat_array_create(&flat_conf);
    //g_id_to_name = chuncked_array_create(&chuncked_conf);  // 块大小 256
    gc_pthread_mutex_init(&g_name_lock, NULL);
    gc_add_root(&g_name_lock);
    // 加入一个占位 ID 0（无效 ID）
    //chuncked_array_add(g_id_to_name, (uint64_t)NULL);
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
    //uint64_t new_id = chuncked_array_length(g_id_to_name);
    uint64_t new_id = flat_array_length(g_id_to_name);
    char *name_copy = strdup(name);
    //chuncked_array_add(g_id_to_name, (uint64_t)name_copy);
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
/*
    if (id == 0 || id >= chuncked_array_length(g_id_to_name))
        return NULL;
    return (const char*)chuncked_array_get(g_id_to_name, id);
*/
}
