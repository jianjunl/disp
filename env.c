
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

//#define DISP_ENV_HASHING

#ifndef DISP_ENV_HASHING

#include "btree/btree.h"

static inline void* bt_malloc_gc(size_t size) {
    return gc_typed_malloc(size, &GC_TYPE_PTR_ARRAY);
}
static inline void* bt_calloc_gc(size_t nmemb, size_t size) {
    return gc_typed_calloc(nmemb, size, &GC_TYPE_PTR_ARRAY);
}
static inline void bt_free_gc(void *ptr) {
    return gc_free(ptr);
}
// 默认比较函数（数值比较）
static inline int bt_cmp(bt_key_t a, bt_key_t b) {
    return a < b ? -1 : a > b ? 1 : 0;
}

static bt_conf_t bt_conf_gc = (bt_conf_t) {
    .malloc = bt_malloc_gc,
    .calloc = bt_calloc_gc,
    .free   = bt_free_gc,
    .cmp    = bt_cmp,
    .tier   = 3              // 最小度数（每个节点至少有 t-1 个键）
};

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

disp_env_t* disp_new_env(disp_env_t *parent) {
    disp_env_t *env = gc_typed_malloc(sizeof(disp_env_t), &struct_disp_env_ti);
    gc_pthread_mutex_init(&env->lock, NULL);
    env->symbols = btree_create(&bt_conf_gc);
    env->parent = parent;
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
disp_val disp_find_symbol(const disp_env_t *env, disp_sid id) {
    while (env) {
        env_lock(env);
#if DISP_BOXING
        disp_val sym = (disp_val) { .x = btree_search(env->symbols, id.id) };
#else  // DISP_BOXING
        disp_val sym = (disp_val) { .x = btree_search(env->symbols, id.id), .flag = FLAG_SYMBOL };
#endif // DISP_BOXING
        env_unlock(env);
        if (sym.x != VNULL) return sym;
        env = env->parent;
    }
    return NIL;
}

// 定义符号
disp_val disp_define_symbol(const disp_env_t *env, disp_sid id, disp_val value, int final) {
    env_lock(env);
#if DISP_BOXING
    disp_val existing = (disp_val) { .x = btree_search(env->symbols, id.id) };
#else  // DISP_BOXING
    disp_val existing = (disp_val) { .x = btree_search(env->symbols, id.id), .flag = FLAG_SYMBOL };
#endif // DISP_BOXING
    if (existing.x != VNULL) {
        // 符号已存在，更新其值（如果允许 final 等逻辑）
        if (!final) {  // 假设 final 标志表示不可修改
            disp_set_symbol_value_unlock(env, existing, value);
        }
        env_unlock(env);
        return existing;
    }
    // 创建新符号
    disp_val sym = disp_make_symbol(id);
    btree_insert(env->symbols, id.id, sym.x);
    disp_set_symbol_value_unlock(env, sym, value);
    env_unlock(env);
    return sym;
}

disp_val disp_intern_symbol(const disp_env_t *env, disp_sid id) {
    env_lock(env);
#if DISP_BOXING
    disp_val existing = (disp_val) { .x = btree_search(env->symbols, id.id) };
#else  // DISP_BOXING
    disp_val existing = (disp_val) { .x = btree_search(env->symbols, id.id), .flag = FLAG_SYMBOL };
#endif // DISP_BOXING
    if (existing.x != VNULL) {
        env_unlock(env);
        return existing;
    }
    // 创建新符号
    disp_val sym = disp_make_symbol(id);
    btree_insert(env->symbols, id.id, sym.x);
    disp_set_symbol_value_unlock(env, sym, NIL);
    env_unlock(env);
    return sym;
}

disp_val disp_find_symbol_by_name(const disp_env_t *env, const char *name) {
    disp_sid id = disp_get_sid(name);
    return disp_find_symbol(env, id);
}

disp_val disp_define_symbol_by_name(const disp_env_t *env, const char *name, disp_val value, int final) {
    disp_sid id = disp_get_sid(name);
    return disp_define_symbol(env, id, value, final);
}

disp_val disp_intern_symbol_by_name(const disp_env_t *env, const char *name) {
    disp_sid id = disp_get_sid(name);
    return disp_intern_symbol(env, id);
}

inline void disp_set_symbol_value(const disp_env_t *env, disp_val sym, disp_val value) {
    env_lock(env);
    disp_set_symbol_value(env, sym, value);
    env_unlock(env);
}

inline bool disp_update_symbol(const disp_env_t *env, disp_val sym) {
    return btree_update(env->symbols, SYM_ID(sym).id, sym.x);
}

/* ======================== GC 初始化和全局常量 ======================== */

void disp_init_env() {

    disp_global_env = gc_typed_malloc(sizeof(disp_env_t), &struct_disp_env_ti);
    gc_pthread_mutex_init(&disp_global_env->lock, NULL);
    disp_global_env->symbols = btree_create(&bt_conf_gc);

    gc_add_root(&disp_global_env);
}

#else // DISP_ENV_HASHING

struct sym_entry {
    disp_val symbol;
    struct sym_entry *next;
    disp_sid id;
    int final;
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

disp_val disp_find_symbol(const disp_env_t *env, disp_sid id) {
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

disp_val disp_define_symbol(const disp_env_t *env, disp_sid id, disp_val value, int final) {
    env_lock(env);
    unsigned int idx = id % SYM_TABLE_SIZE;
    struct sym_entry *e = env->buckets[idx];
    while (e) {
        if (e->id == id) {
            if (e->final) {
                DBG("  trying to update final symbol id %llu\n", (unsigned long long)id);
            } else {
                disp_set_symbol_value_unlock(env, e->symbol, value);
            }
            env_unlock(env);
            return e->symbol;
        }
        e = e->next;
    }
    
    // 创建新符号
    disp_val sym = disp_make_symbol(id);
    disp_set_symbol_value_unlock(env, sym, value);
    
    struct sym_entry *new_entry = gc_typed_malloc(sizeof(struct sym_entry), &struct_sym_entry_ti);
    new_entry->id = id;
    GC_ASSIGN_PTR(new_entry->symbol, sym);
    new_entry->final = final;
    GC_ASSIGN_PTR(new_entry->next, env->buckets[idx]);
    GC_ASSIGN_PTR(env->buckets[idx], new_entry);
    
    env_unlock(env);
    return sym;
}

disp_val disp_intern_symbol(const disp_env_t *env, disp_sid id) {
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
    disp_val sym = disp_make_symbol(id);
    disp_set_symbol_value_unlock(env, sym, NIL);
    
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
    disp_sid id = disp_get_sid(name);
    return disp_find_symbol(env, id);
}

disp_val disp_define_symbol_by_name(const disp_env_t *env, const char *name, disp_val value, int final) {
    disp_sid id = disp_get_sid(name);
    return disp_define_symbol(env, id, value, final);
}

disp_val disp_intern_symbol_by_name(const disp_env_t *env, const char *name) {
    disp_sid id = disp_get_sid(name);
    return disp_intern_symbol(env, id);
}

inline bool disp_update_symbol(const disp_env_t *env, disp_val sym) {
    (void)env;(void)sym;
    return true;
}


/* ======================== GC 初始化和全局常量 ======================== */

void disp_init_env() {

    disp_global_env = gc_typed_calloc(1, sizeof(struct disp_env), &struct_disp_env_ti);
    gc_pthread_mutex_init(&disp_global_env->lock, NULL);
    disp_global_env->buckets = gc_typed_calloc(SYM_TABLE_SIZE, sizeof(void*), &GC_TYPE_PTR_ARRAY);;

    gc_add_root(&disp_global_env);
}

#endif // DISP_BOXING
