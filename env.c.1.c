
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

#if DISP_BOXING

// ---------- 键辅助函数（uint64_t 堆存储）----------
static int id_cmp(const void *a, const void *b) {
    uint64_t va = *(const uint64_t*)a;
    uint64_t vb = *(const uint64_t*)b;
    return (va < vb) ? -1 : (va > vb) ? 1 : 0;
}

static void* id_dup(const void *key) {
    uint64_t *p = malloc(sizeof(uint64_t));
    if (p) *p = *(const uint64_t*)key;
    return p;
}

static void id_free(void *key) {
    free(key);
}

disp_env_t* disp_new_env(disp_env_t *parent) {
    disp_env_t *env = gc_typed_malloc(sizeof(disp_env_t), &struct_disp_env_ti);
    gc_pthread_mutex_init(&env->lock, NULL);
    // 使用 GC 版本的 btree，传入键管理函数
    env->symbols = btree_create_gc(3, id_cmp, id_dup, id_free);
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
disp_val disp_find_symbol(const disp_env_t *env, uint64_t id) {
    while (env) {
        env_lock(env);
        // 直接传递 id 的地址（btree 内部会调用 id_dup 复制）
        disp_val sym = (disp_val)(uintptr_t)btree_search(env->symbols, &id);
        env_unlock(env);
        if (sym != (disp_val)0) return sym;
        env = env->parent;
    }
    return NIL;
}

// 定义符号
disp_val disp_define_symbol(const disp_env_t *env, uint64_t id, disp_val value, int final) {
    env_lock(env);
    disp_val existing = (disp_val)(uintptr_t)btree_search(env->symbols, &id);
    if (existing != (disp_val)0) {
        if (!final) {
            disp_set_symbol_value_unlock(existing, value);
        }
        env_unlock(env);
        return existing;
    }
    // 创建新符号
    disp_val sym = disp_make_symbol(id);
    disp_set_symbol_value_unlock(sym, value);
    btree_insert(env->symbols, &id, (void*)(uintptr_t)sym);
    env_unlock(env);
    return sym;
}

disp_val disp_intern_symbol(const disp_env_t *env, uint64_t id) {
    env_lock(env);
    disp_val existing = (disp_val)(uintptr_t)btree_search(env->symbols, &id);
    if (existing != (disp_val)0) {
        env_unlock(env);
        return existing;
    }
    disp_val sym = disp_make_symbol(id);
    disp_set_symbol_value_unlock(sym, NIL);
    btree_insert(env->symbols, &id, (void*)(uintptr_t)sym);
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
    disp_global_env->symbols = btree_create_gc(3, id_cmp, id_dup, id_free);
    gc_add_root(&disp_global_env);
}

#else // DISP_BOXING

// ---------- 键辅助函数（uint64_t 堆存储，同 boxing 分支）----------
static int id_cmp(const void *a, const void *b) {
    uint64_t va = *(const uint64_t*)a;
    uint64_t vb = *(const uint64_t*)b;
    return (va < vb) ? -1 : (va > vb) ? 1 : 0;
}

static void* id_dup(const void *key) {
    uint64_t *p = malloc(sizeof(uint64_t));
    if (p) *p = *(const uint64_t*)key;
    return p;
}

static void id_free(void *key) {
    free(key);
}

// 符号值在非 boxing 模式下是结构体，需要存储其指针
// 符号对象本身由 GC 分配（通过 disp_make_symbol 返回的是结构体值，
// 但为了存储其指针，我们需要将符号分配在堆上。实际上 disp_make_symbol
// 在非 boxing 模式下返回的是结构体，不能直接取地址（可能为临时值）。
// 因此我们将符号分配在 GC 堆上，通过 gc_typed_malloc 获得稳定的指针。
// 约定：btree 中存储的 value 是指向符号结构体的指针（disp_val*），
// 查找时返回解引用的结构体。

// 辅助：创建 GC 托管的符号副本（深拷贝，因为符号结构体内可能包含指针数据）
static disp_val* make_symbol_on_heap(uint64_t id, disp_val value) {
    disp_val *sym_ptr = gc_typed_malloc(sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    *sym_ptr = disp_make_symbol(id);
    disp_set_symbol_value_unlock(*sym_ptr, value);
    return sym_ptr;
}

disp_env_t* disp_new_env(disp_env_t *parent) {
    disp_env_t *env = gc_typed_malloc(sizeof(disp_env_t), &struct_disp_env_ti);
    gc_pthread_mutex_init(&env->lock, NULL);
    env->symbols = btree_create_gc(3, id_cmp, id_dup, id_free);
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

// 查找符号（返回结构体）
disp_val disp_find_symbol(const disp_env_t *env, uint64_t id) {
    while (env) {
        env_lock(env);
        disp_val *sym_ptr = (disp_val*)btree_search(env->symbols, &id);
        env_unlock(env);
        if (sym_ptr != NULL) return *sym_ptr;
        env = env->parent;
    }
    return DNULL;
}

// 定义符号
disp_val disp_define_symbol(const disp_env_t *env, uint64_t id, disp_val value, int final) {
    env_lock(env);
    disp_val *existing_ptr = (disp_val*)btree_search(env->symbols, &id);
    if (existing_ptr != NULL) {
        if (!final) {
            disp_set_symbol_value_unlock(*existing_ptr, value);
        }
        env_unlock(env);
        return *existing_ptr;
    }
    // 创建新符号（堆上分配）
    disp_val *sym_ptr = make_symbol_on_heap(id, value);
    btree_insert(env->symbols, &id, sym_ptr);
    env_unlock(env);
    return *sym_ptr;
}

disp_val disp_intern_symbol(const disp_env_t *env, uint64_t id) {
    env_lock(env);
    disp_val *existing_ptr = (disp_val*)btree_search(env->symbols, &id);
    if (existing_ptr != NULL) {
        env_unlock(env);
        return *existing_ptr;
    }
    disp_val *sym_ptr = make_symbol_on_heap(id, NIL);
    btree_insert(env->symbols, &id, sym_ptr);
    env_unlock(env);
    return *sym_ptr;
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
    disp_global_env->symbols = btree_create_gc(3, id_cmp, id_dup, id_free);
    gc_add_root(&disp_global_env);
}

#endif // DISP_BOXING
