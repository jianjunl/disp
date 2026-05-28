
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

struct disp_scope {
    struct sym_entry **buckets;
    gc_mutex_t *lock;
    struct disp_scope *parent;
};

GC_STRUCT_TI(disp_scope,
    GC_OFF(disp_scope, buckets),
    GC_OFF(disp_scope, lock),
    GC_OFF(disp_scope, parent)
);

disp_scope_t *disp_global_scope = NULL;

static void scope_lock(const disp_scope_t *scope) {
    if(scope) gc_pthread_mutex_lock(scope->lock);
    else gc_pthread_mutex_lock(disp_global_scope->lock);
}

static void scope_unlock(const disp_scope_t *scope) {
    if(scope) gc_pthread_mutex_unlock(scope->lock);
    else gc_pthread_mutex_unlock(disp_global_scope->lock);
}

disp_scope_t* disp_new_scope(disp_scope_t *parent) {
    if (!parent) parent = disp_global_scope;
    disp_scope_t *t = gc_typed_malloc(sizeof(struct disp_scope), &struct_disp_scope_ti);
    gc_pthread_mutex_init(&t->lock, NULL);
    t->buckets = gc_typed_calloc(SYM_TABLE_SIZE, sizeof(void*), &GC_TYPE_PTR_ARRAY);;
    t->parent = parent;
    return t;
}

disp_scope_t* disp_dup_scope(disp_scope_t *old) {
    if (!old) return old;
    disp_scope_t *t = gc_typed_malloc(sizeof(struct disp_scope), &struct_disp_scope_ti);
    t->lock    = old->lock;
    t->buckets = old->buckets;
    t->parent  = old->parent;
    return t;
}

disp_val disp_find_symbol_by_id(const disp_scope_t *scope, uint64_t id) {
    if (!scope) scope = disp_global_scope;
    while (scope) {
        scope_lock(scope);
        unsigned int idx = id % SYM_TABLE_SIZE;   // 直接用 id 作为哈希
        struct sym_entry *e = scope->buckets[idx];
        while (e) {
            if (e->id == id) {
                scope_unlock(scope);
                return e->symbol;
            }
            e = e->next;
        }
        scope_unlock(scope);
        scope = scope->parent;
    }
    return DNULL;
}

disp_val disp_define_symbol_by_id(const disp_scope_t *scope, uint64_t id, disp_val value, int final) {
    if (!scope) scope = disp_global_scope;
    scope_lock(scope);
    unsigned int idx = id % SYM_TABLE_SIZE;
    struct sym_entry *e = scope->buckets[idx];
    while (e) {
        if (e->id == id) {
            if (e->final) {
                DBG("  trying to update final symbol id %llu\n", (unsigned long long)id);
            } else {
                disp_set_symbol_value(e->symbol, value);
            }
            scope_unlock(scope);
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
    GC_ASSIGN_PTR(new_entry->next, scope->buckets[idx]);
    GC_ASSIGN_PTR(scope->buckets[idx], new_entry);
    
    scope_unlock(scope);
    return sym;
}

disp_val disp_intern_symbol_by_id(const disp_scope_t *scope, uint64_t id) {
    if (!scope) scope = disp_global_scope;
    scope_lock(scope);
    unsigned int idx = id % SYM_TABLE_SIZE;
    struct sym_entry *e = scope->buckets[idx];
    while (e) {
        if (e->id == id) {
            scope_unlock(scope);
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
    GC_ASSIGN_PTR(new_entry->next, scope->buckets[idx]);
    GC_ASSIGN_PTR(scope->buckets[idx], new_entry);
    
    scope_unlock(scope);
    return sym;
}

disp_val disp_find_symbol(const disp_scope_t *scope, const char *name) {
    uint64_t id = disp_get_id(name);
    return disp_find_symbol_by_id(scope, id);
}

disp_val disp_define_symbol(const disp_scope_t *scope, const char *name, disp_val value, int final) {
    uint64_t id = disp_get_id(name);
    return disp_define_symbol_by_id(scope, id, value, final);
}

disp_val disp_intern_symbol(const disp_scope_t *scope, const char *name) {
    uint64_t id = disp_get_id(name);
    return disp_intern_symbol_by_id(scope, id);
}

/* ======================== GC 初始化和全局常量 ======================== */

void disp_init_scope() {

    disp_global_scope = gc_typed_calloc(1, sizeof(struct disp_scope), &struct_disp_scope_ti);
    gc_pthread_mutex_init(&disp_global_scope->lock, NULL);
    disp_global_scope->buckets = gc_typed_calloc(SYM_TABLE_SIZE, sizeof(void*), &GC_TYPE_PTR_ARRAY);;

    gc_add_root(&disp_global_scope);
}
