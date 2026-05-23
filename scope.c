
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

GC_STRUCT_TI(disp_val,
    GC_OFF(disp_val, data)
);

struct sym_entry {
    char *name;
    disp_val *symbol;
    int final;
    struct sym_entry *next;
};

GC_STRUCT_TI(sym_entry,
    GC_OFF(sym_entry, name),
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

static unsigned int hash(const char *s) {
    unsigned int h = 0;
    while (*s) h = h * 31 + (unsigned char)*s++;
    return h % SYM_TABLE_SIZE;
}

static void scope_lock(const disp_scope_t *scope) {
    if(scope) gc_pthread_mutex_lock(scope->lock);
    else gc_pthread_mutex_lock(disp_global_scope->lock);
}

static void scope_unlock(const disp_scope_t *scope) {
    if(scope) gc_pthread_mutex_unlock(scope->lock);
    else gc_pthread_mutex_unlock(disp_global_scope->lock);
}

/* ======================== 对象分配 ======================== */
disp_val* disp_alloc(disp_flag_t flag, disp_data *data) {
    disp_val *v = gc_typed_calloc(1, sizeof(struct disp_val), &struct_disp_val_ti);
    v->data = data;
    v->flag = flag;
    return v;
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

disp_val* disp_find_symbol(const disp_scope_t *scope, const char *name) {
    if (!scope) scope = disp_global_scope;
    while (scope) {
        scope_lock(scope);
        unsigned int idx = hash(name);
        struct sym_entry *e = scope->buckets[idx];
        while (e) {
            if (strcmp(e->name, name) == 0) {
                scope_unlock(scope);
                return e->symbol;
            }
            e = e->next;
        }
        scope_unlock(scope);
        scope = scope->parent;
    }
    return NULL;
}

disp_val* disp_define_symbol(const disp_scope_t *scope, const char *name, disp_val *value, int final) {
    if (!scope) scope = disp_global_scope;
    DBG("disp_define_symbol: %s\n", name);
    scope_lock(scope);
    unsigned int idx = hash(name);
    struct sym_entry *e = scope->buckets[idx];
    while (e) {
        if (strcmp(e->name, name) == 0) {
            if (e->final) {
                DBG("  trying to update final symbol %s\n", name);
            } else {
                disp_set_symbol_value(e->symbol, value);
                DBG("  updated symbol %s -> %p\n", name, (void*)value);
            }
            scope_unlock(scope);
            return e->symbol;
        }
        e = e->next;
    }
    
    // 创建新符号
    disp_val *sym = disp_make_symbol(name);
    disp_set_symbol_value(sym, value);
    
    struct sym_entry *new_entry = gc_typed_malloc(sizeof(struct sym_entry), &struct_sym_entry_ti);
    GC_ASSIGN_PTR(new_entry->name, gc_strdup(name));
    GC_ASSIGN_PTR(new_entry->symbol, sym);
    new_entry->final = final;
    GC_ASSIGN_PTR(new_entry->next, scope->buckets[idx]);
    GC_ASSIGN_PTR(scope->buckets[idx], new_entry);
    
    scope_unlock(scope);
    return sym;
}

disp_val* disp_intern_symbol(const disp_scope_t *scope, const char *name) {
    if (!scope) scope = disp_global_scope;
    scope_lock(scope);
    unsigned int idx = hash(name);
    struct sym_entry *e = scope->buckets[idx];
    while (e) {
        if (strcmp(e->name, name) == 0) {
            scope_unlock(scope);
            return e->symbol;
        }
        e = e->next;
    }
    
    // 未找到，创建新符号
    disp_val *sym = disp_make_symbol(name);
    
    struct sym_entry *new_entry = gc_typed_malloc(sizeof(struct sym_entry), &struct_sym_entry_ti);
    GC_ASSIGN_PTR(new_entry->name, gc_strdup(name));
    GC_ASSIGN_PTR(new_entry->symbol, sym);
    new_entry->final = 0;
    GC_ASSIGN_PTR(new_entry->next, scope->buckets[idx]);
    GC_ASSIGN_PTR(scope->buckets[idx], new_entry);
    
    scope_unlock(scope);
    return sym;
}

/* ======================== GC 初始化和全局常量 ======================== */

void disp_init_scope() {

    disp_global_scope = gc_typed_calloc(1, sizeof(struct disp_scope), &struct_disp_scope_ti);
    gc_pthread_mutex_init(&disp_global_scope->lock, NULL);
    disp_global_scope->buckets = gc_typed_calloc(SYM_TABLE_SIZE, sizeof(void*), &GC_TYPE_PTR_ARRAY);;

    gc_add_root(&disp_global_scope);
}
