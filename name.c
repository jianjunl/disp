
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "disp.h"
#include "array.h"

static disp_array_t *g_name_to_id = NULL;   // 存储字符串指针 (uint64_t 强转为 char*)
static disp_array_t *g_id_to_name = NULL;   // 存储字符串指针
static gc_mutex_t *g_name_lock;

// 初始化全局名称表
void disp_init_name_table(void) {
    g_name_to_id = disp_array_create(256);   // 块大小 256
    g_id_to_name = disp_array_create(256);
    gc_pthread_mutex_init(&g_name_lock, NULL);
    gc_add_root(&g_name_lock);
    gc_add_root(&g_name_to_id);
    gc_add_root(&g_id_to_name);
    // 加入一个占位 ID 0（无效 ID）
    disp_array_add(g_name_to_id, (uint64_t)NULL);
    disp_array_add(g_id_to_name, (uint64_t)NULL);
}

// 获取或分配 ID（线程安全）
uint64_t disp_get_id(const char *name) {
    if (!name) return 0;
    gc_pthread_mutex_lock(g_name_lock);
    
    // 线性搜索（因为数组不大，且只增，可优化为哈希表，但先简单实现）
    size_t len = disp_array_length(g_name_to_id);
    for (size_t i = 1; i < len; i++) {  // 跳过索引 0
        const char *existing = (const char*)disp_array_get(g_name_to_id, i);
        if (existing && strcmp(existing, name) == 0) {
            gc_pthread_mutex_unlock(g_name_lock);
            return i;
        }
    }
    
    // 未找到，分配新 ID
    uint64_t new_id = len;
    //char *name_copy = gc_strdup(name);
    char *name_copy = strdup(name);
    disp_array_add(g_name_to_id, (uint64_t)name_copy);
    disp_array_add(g_id_to_name, (uint64_t)name_copy);
    gc_pthread_mutex_unlock(g_name_lock);
    return new_id;
}

// 根据 ID 获取名称（用于调试或打印）
const char* disp_get_name(uint64_t id) {
    if (id == 0 || id >= disp_array_length(g_id_to_name))
        return NULL;
    return (const char*)disp_array_get(g_id_to_name, id);
}
