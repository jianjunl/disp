
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

union disp_data {
    struct {
        char *name;
        disp_val *value;
    } symbol;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, symbol.name),
    GC_OFF(disp_data, symbol.value)
);

/* ======================== 符号表 ======================== */
#define SYM_TABLE_SIZE 1024

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

static struct sym_entry *sym_buckets[SYM_TABLE_SIZE]  __attribute__((section("gc_roots")));

static gc_mutex_t sym_lock = GC_PTHREAD_MUTEX_INITIALIZER;

static unsigned int hash(const char *s) {
    unsigned int h = 0;
    while (*s) h = h * 31 + (unsigned char)*s++;
    return h % SYM_TABLE_SIZE;
}

void disp_lock_table(void) {
    gc_pthread_mutex_lock(&sym_lock);
}

void disp_unlock_table(void) {
    gc_pthread_mutex_unlock(&sym_lock);
}

/* ======================== 对象分配 ======================== */
disp_val* disp_alloc(disp_flag_t flag, disp_data *data) {
    disp_val *v = gc_typed_calloc(1, sizeof(struct disp_val), &struct_disp_val_ti);
    v->data = data;
    v->flag = flag;
    return v;
}

/* ======================== 符号表操作 ======================== */

static disp_val* make_symbol(const char *name) {
    disp_val *v = DISP_ALLOC_TI(DISP_SYMBOL);
    if (!v) return NULL;
    
    // 初始化符号数据
    GC_ASSIGN_PTR(v->data->symbol.name, gc_strdup(name));
    GC_ASSIGN_PTR(v->data->symbol.value, NIL);
    
    return v;
}

disp_val* disp_find_symbol(const char *name) {
    disp_lock_table();
    unsigned int idx = hash(name);
    struct sym_entry *e = sym_buckets[idx];
    while (e) {
        if (strcmp(e->name, name) == 0) {
            disp_unlock_table();
            return e->symbol;
        }
        e = e->next;
    }
    disp_unlock_table();
    return NULL;
}

disp_val* disp_define_symbol(const char *name, disp_val *value, int final) {
    DBG("disp_define_symbol: %s\n", name);
    disp_lock_table();
    unsigned int idx = hash(name);
    struct sym_entry *e = sym_buckets[idx];
    while (e) {
        if (strcmp(e->name, name) == 0) {
            if (e->final) {
                DBG("  trying to update final symbol %s\n", name);
            } else {
                GC_ASSIGN_PTR(e->symbol->data->symbol.value, value);
                DBG("  updated symbol %s -> %p\n", name, (void*)value);
            }
            disp_unlock_table();
            return e->symbol;
        }
        e = e->next;
    }
    
    // 创建新符号
    disp_val *sym = make_symbol(name);
    GC_ASSIGN_PTR(sym->data->symbol.value, value);
    
    struct sym_entry *new_entry = gc_typed_malloc(sizeof(struct sym_entry), &struct_sym_entry_ti);
    GC_ASSIGN_PTR(new_entry->name, gc_strdup(name));
    GC_ASSIGN_PTR(new_entry->symbol, sym);
    new_entry->final = final;
    GC_ASSIGN_PTR(new_entry->next, sym_buckets[idx]);
    GC_ASSIGN_PTR(sym_buckets[idx], new_entry);
    
    disp_unlock_table();
    return sym;
}

disp_val* disp_intern_symbol(const char *name) {
    disp_lock_table();
    unsigned int idx = hash(name);
    struct sym_entry *e = sym_buckets[idx];
    while (e) {
        if (strcmp(e->name, name) == 0) {
            disp_unlock_table();
            return e->symbol;
        }
        e = e->next;
    }
    
    // 未找到，创建新符号
    disp_val *sym = make_symbol(name);
    
    struct sym_entry *new_entry = gc_typed_malloc(sizeof(struct sym_entry), &struct_sym_entry_ti);
    GC_ASSIGN_PTR(new_entry->name, gc_strdup(name));
    GC_ASSIGN_PTR(new_entry->symbol, sym);
    new_entry->final = 0;
    GC_ASSIGN_PTR(new_entry->next, sym_buckets[idx]);
    GC_ASSIGN_PTR(sym_buckets[idx], new_entry);
    
    disp_unlock_table();
    return sym;
}

char* disp_get_symbol_name(disp_val *v) {
    if (!v || v->flag != DISP_SYMBOL) {
        ERRO("disp_get_symbol_name failed");
    }
    return v->data->symbol.name;
}

disp_val* disp_get_symbol_value(disp_val *v) {
    if (!v || v->flag != DISP_SYMBOL) {
        ERRO("disp_get_symbol_value failed");
    }
    return v->data->symbol.value;
}

/* ======================== GC 初始化和全局常量 ======================== */

void disp_init_symbol() {
    NIL  = DISP_ALLOC_TI(DISP_VOID);
    TRUE = DISP_ALLOC_TI(DISP_VOID);
    QUIT = DISP_ALLOC_TI(DISP_VOID);
    DEF("nil",   NIL,  1);
    DEF("false", NIL,  1);
    DEF("true",  TRUE, 1);
    DEF("quit",  QUIT, 1);
    DEF("q",     QUIT, 1);
    DEF(":q",    QUIT, 1);
}
