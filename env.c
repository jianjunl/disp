
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <ucontext.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"
#include "btree/btree.h"

struct disp_env {
    btree_t *symbols;      // 符号表：id -> disp_val (symbol)
    gc_mutex_t *lock;
    struct disp_env *parent;
};

GC_STRUCT_TI(disp_env,
    GC_OFF(disp_env, symbols),
    GC_OFF(disp_env, lock),
    GC_OFF(disp_env, parent)
);

// 比较函数（符号 ID 比较）
static int id_cmp(uint64_t a, uint64_t b) {
    return (a < b) ? -1 : (a > b) ? 1 : 0;
}

disp_env_t* disp_new_env(disp_env_t *parent) {
    disp_env_t *env = gc_typed_malloc(sizeof(disp_env_t), &struct_disp_env_ti);
    gc_pthread_mutex_init(&env->lock, NULL);
    env->symbols = btree_create_gc(3, id_cmp);   // 最小度数 3
    env->parent = parent;
    // 根保护：可将整个树的根注册，但 btree 内部已注册根节点，这里不再重复
    return env;
}

disp_env_t *disp_global_env = NULL;

static void env_lock(const disp_env_t *env) {
    if(env) gc_pthread_mutex_lock(env->lock);
    else gc_pthread_mutex_lock(disp_global_env->lock);
}

static void env_unlock(const disp_env_t *env) {
    if(env) gc_pthread_mutex_unlock(env->lock);
    else gc_pthread_mutex_unlock(disp_global_env->lock);
}

disp_env_t* disp_dup_env(disp_env_t *old) {
    if (!old) return old;
    disp_env_t *t = gc_typed_malloc(sizeof(struct disp_env), &struct_disp_env_ti);
    t->lock    = old->lock;
    t->symbols = old->symbols;
    t->parent  = old->parent;
    return t;
}

// 查找符号
disp_val disp_find_symbol(const disp_env_t *env, uint64_t id) {
    while (env) {
        env_lock(env);
        disp_val sym = (disp_val)(uintptr_t)btree_search(env->symbols, id);
        env_unlock(env);
        if (sym != (disp_val)0) return sym;
        env = env->parent;
    }
    return NIL;
}

// 定义符号
disp_val disp_define_symbol(const disp_env_t *env, uint64_t id, disp_val value, int final) {
    env_lock(env);
    disp_val existing = (disp_val)(uintptr_t)btree_search(env->symbols, id);
    if (existing != (disp_val)0) {
        // 符号已存在，更新其值（如果允许 final 等逻辑）
        if (!final) {  // 假设 final 标志表示不可修改
            disp_set_symbol_value(existing, value);
        }
        env_unlock(env);
        return existing;
    }
    // 创建新符号
    disp_val sym = disp_make_symbol(id);
    disp_set_symbol_value(sym, value);
    btree_insert(env->symbols, id, (void*)(uintptr_t)sym);
    env_unlock(env);
    return sym;
}

disp_val disp_intern_symbol(const disp_env_t *env, uint64_t id) {
    env_lock(env);
    disp_val existing = (disp_val)(uintptr_t)btree_search(env->symbols, id);
    if (existing != (disp_val)0) {
        env_unlock(env);
        return existing;
    }
    // 创建新符号
    disp_val sym = disp_make_symbol(id);
    disp_set_symbol_value(sym, NIL);
    btree_insert(env->symbols, id, (void*)(uintptr_t)sym);
    env_unlock(env);
    return sym;
}

disp_val disp_find_symbol_by_name(const disp_env_t *env, const char *name) {
    uint64_t id = disp_get_id(name);
    return disp_find_symbol(env, id);
}

disp_val disp_define_symbol_by_name(const disp_env_t *env, const char *name, disp_val value, int final) {
    uint64_t id = disp_get_id(name);
    return disp_define_symbol(env, id, value, final);
}

disp_val disp_intern_symbol_by_name(const disp_env_t *env, const char *name) {
    uint64_t id = disp_get_id(name);
    return disp_intern_symbol(env, id);
}

/* ======================== GC 初始化和全局常量 ======================== */

void disp_init_env() {

    disp_global_env = gc_typed_malloc(sizeof(disp_env_t), &struct_disp_env_ti);
    gc_pthread_mutex_init(&disp_global_env->lock, NULL);
    disp_global_env->symbols = btree_create_gc(3, id_cmp);   // 最小度数 3

    gc_add_root(&disp_global_env);
}
