
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

struct sym_entry {
    uint64_t id;
    disp_val symbol;
    int final;
    struct sym_entry *next;
};

GC_STRUCT_TI(sym_entry,
    GC_OFF(sym_entry, symbol),
    GC_OFF(sym_entry, next)
);

/* ======================== 符号表 ======================== */
#define SYM_TABLE_SIZE 1024

struct disp_env {
    struct sym_entry **buckets;
    gc_mutex_t *lock;
    struct disp_env *parent;
};

GC_STRUCT_TI(disp_env,
    GC_OFF(disp_env, buckets),
    GC_OFF(disp_env, lock),
    GC_OFF(disp_env, parent)
);

disp_env_t *disp_global_env = NULL;

static void env_lock(const disp_env_t *env) {
    if(env) gc_pthread_mutex_lock(env->lock);
    else gc_pthread_mutex_lock(disp_global_env->lock);
}

static void env_unlock(const disp_env_t *env) {
    if(env) gc_pthread_mutex_unlock(env->lock);
    else gc_pthread_mutex_unlock(disp_global_env->lock);
}

disp_env_t* disp_new_env(disp_env_t *parent) {
    if (!parent) parent = disp_global_env;
    disp_env_t *t = gc_typed_malloc(sizeof(struct disp_env), &struct_disp_env_ti);
    gc_pthread_mutex_init(&t->lock, NULL);
    t->buckets = gc_typed_calloc(SYM_TABLE_SIZE, sizeof(void*), &GC_TYPE_PTR_ARRAY);;
    t->parent = parent;
    return t;
}

disp_env_t* disp_dup_env(disp_env_t *old) {
    if (!old) return old;
    disp_env_t *t = gc_typed_malloc(sizeof(struct disp_env), &struct_disp_env_ti);
    t->lock    = old->lock;
    t->buckets = old->buckets;
    t->parent  = old->parent;
    return t;
}

disp_val disp_find_symbol(const disp_env_t *env, uint64_t id) {
    if (!env) env = disp_global_env;
    while (env) {
        env_lock(env);
        unsigned int idx = id % SYM_TABLE_SIZE;   // 直接用 id 作为哈希
        struct sym_entry *e = env->buckets[idx];
        while (e) {
            if (e->id == id) {
                env_unlock(env);
                return e->symbol;
            }
            e = e->next;
        }
        env_unlock(env);
        env = env->parent;
    }
    return DNULL;
}

disp_val disp_define_symbol(const disp_env_t *env, uint64_t id, disp_val value, int final) {
    if (!env) env = disp_global_env;
    env_lock(env);
    unsigned int idx = id % SYM_TABLE_SIZE;
    struct sym_entry *e = env->buckets[idx];
    while (e) {
        if (e->id == id) {
            if (e->final) {
                DBG("  trying to update final symbol id %llu\n", (unsigned long long)id);
            } else {
                disp_set_symbol_value(e->symbol, value);
            }
            env_unlock(env);
            return e->symbol;
        }
        e = e->next;
    }
    
    // 创建新符号
    disp_val sym = disp_make_symbol(disp_get_name(id));  // 需要名称，但这里可能没有，我们可传入临时名
    // 更好的做法：提前通过 id 获取 name，但 id 已知时通常 name 已存在全局表
    // 若无法获取 name，可暂时传 "?"，但正常情况下不会发生
    // 所以我们需要一个从 id 到 name 的映射，即 disp_get_name(id)
    disp_set_symbol_value(sym, value);
    
    struct sym_entry *new_entry = gc_typed_malloc(sizeof(struct sym_entry), &struct_sym_entry_ti);
    new_entry->id = id;
    GC_ASSIGN_PTR(new_entry->symbol, sym);
    new_entry->final = final;
    GC_ASSIGN_PTR(new_entry->next, env->buckets[idx]);
    GC_ASSIGN_PTR(env->buckets[idx], new_entry);
    
    env_unlock(env);
    return sym;
}

disp_val disp_intern_symbol(const disp_env_t *env, uint64_t id) {
    if (!env) env = disp_global_env;
    env_lock(env);
    unsigned int idx = id % SYM_TABLE_SIZE;
    struct sym_entry *e = env->buckets[idx];
    while (e) {
        if (e->id == id) {
            env_unlock(env);
            return e->symbol;
        }
        e = e->next;
    }
    
    // 未找到，创建新符号
    const char *name = disp_get_name(id);
    disp_val sym = disp_make_symbol(name ? name : "?");
    disp_set_symbol_value(sym, NIL);
    
    struct sym_entry *new_entry = gc_typed_malloc(sizeof(struct sym_entry), &struct_sym_entry_ti);
    new_entry->id = id;
    GC_ASSIGN_PTR(new_entry->symbol, sym);
    new_entry->final = 0;
    GC_ASSIGN_PTR(new_entry->next, env->buckets[idx]);
    GC_ASSIGN_PTR(env->buckets[idx], new_entry);
    
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

    disp_global_env = gc_typed_calloc(1, sizeof(struct disp_env), &struct_disp_env_ti);
    gc_pthread_mutex_init(&disp_global_env->lock, NULL);
    disp_global_env->buckets = gc_typed_calloc(SYM_TABLE_SIZE, sizeof(void*), &GC_TYPE_PTR_ARRAY);;

    gc_add_root(&disp_global_env);
}
