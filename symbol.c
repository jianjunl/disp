
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

#ifndef DISP_ALLINONE_UNION_H
union disp_data {
    /* 符号 */
    struct {
        char *name;
        disp_val *value;
    } symbol;
};
#endif

/* ======================== 符号表 ======================== */
#define SYM_TABLE_SIZE 1024

struct sym_entry {
    char *name;
    disp_val *symbol;
    int final;
    struct sym_entry *next;
};

struct sym_table {
    struct sym_entry *buckets[SYM_TABLE_SIZE];
    gc_mutex_t lock;
};

static struct sym_table sym_table = { .lock = GC_PTHREAD_MUTEX_INITIALIZER };

static unsigned int hash(const char *s) {
    unsigned int h = 0;
    while (*s) h = h * 31 + (unsigned char)*s++;
    return h % SYM_TABLE_SIZE;
}

void disp_lock_table(void) {
    gc_pthread_mutex_lock(&sym_table.lock);
}

void disp_unlock_table(void) {
    gc_pthread_mutex_unlock(&sym_table.lock);
}

/* ======================== GC 根管理 ======================== */

static void register_symbol_root(struct sym_entry *e) {
    gc_add_root(&e->symbol->data->symbol.value->data);
    gc_add_root(&e->symbol->data->symbol.value);
    gc_add_root(&e->symbol->data->symbol.name);
    gc_add_root(&e->symbol->data);
    gc_add_root(&e->symbol);
    gc_add_root(&e);          // 确保条目本身不被回收
}

/* ======================== 对象分配 ======================== */
disp_val* disp_alloc(disp_flag_t flag, disp_data *data) {
    disp_val *v = gc_calloc(1, sizeof(disp_val));
    v->data = data;
    v->flag = flag;
    return v;
}

/* ======================== 符号表操作 ======================== */

static disp_val* make_symbol(const char *name) {
    disp_val *v = DISP_ALLOC(DISP_SYMBOL);
    if (!v) return NULL;
    
    // 初始化符号数据
    GC_ASSIGN_PTR(v->data->symbol.name, gc_strdup(name));
    GC_ASSIGN_PTR(v->data->symbol.value, NIL);
    
    return v;
}

disp_val* disp_find_symbol(const char *name) {
    disp_lock_table();
    unsigned int idx = hash(name);
    struct sym_entry *e = sym_table.buckets[idx];
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
    struct sym_entry *e = sym_table.buckets[idx];
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
    
    struct sym_entry *new_entry = gc_malloc(sizeof(struct sym_entry));
    GC_ASSIGN_PTR(new_entry->name, gc_strdup(name));
    GC_ASSIGN_PTR(new_entry->symbol, sym);
    new_entry->final = final;
    GC_ASSIGN_PTR(new_entry->next, sym_table.buckets[idx]);
    GC_ASSIGN_PTR(sym_table.buckets[idx], new_entry);
    
    // 注册精确根，确保符号不被回收
    register_symbol_root(new_entry);
    gc_add_root(&sym_table.buckets[idx]);
    
    disp_unlock_table();
    return sym;
}

disp_val* disp_intern_symbol(const char *name) {
    disp_lock_table();
    unsigned int idx = hash(name);
    struct sym_entry *e = sym_table.buckets[idx];
    while (e) {
        if (strcmp(e->name, name) == 0) {
            disp_unlock_table();
            return e->symbol;
        }
        e = e->next;
    }
    
    // 未找到，创建新符号
    disp_val *sym = make_symbol(name);
    
    struct sym_entry *new_entry = gc_malloc(sizeof(struct sym_entry));
    GC_ASSIGN_PTR(new_entry->name, gc_strdup(name));
    GC_ASSIGN_PTR(new_entry->symbol, sym);
    new_entry->final = 0;
    GC_ASSIGN_PTR(new_entry->next, sym_table.buckets[idx]);
    GC_ASSIGN_PTR(sym_table.buckets[idx], new_entry);
    
    register_symbol_root(new_entry);
    gc_add_root(&sym_table.buckets[idx]);
    
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

disp_val *NIL, *TRUE, *QUIT;

void disp_init_gc() {
    gc_init();
    
    disp_init_info();
   
    gc_add_root(&sym_table);
 
    NIL  = DISP_ALLOC(DISP_VOID);
    TRUE = DISP_ALLOC(DISP_VOID);
    QUIT = DISP_ALLOC(DISP_VOID);
    
    gc_add_root(&NIL);
    gc_add_root(&TRUE);
    gc_add_root(&QUIT);
    
    DEF("nil",   NIL,  1);
    DEF("false", NIL,  1);
    DEF("true",  TRUE, 1);
    DEF("quit",  QUIT, 1);
    DEF("q",     QUIT, 1);
    DEF(":q",    QUIT, 1);
}

void disp_gc(void) {
    gc_collect();
    gc_dump_stats();
}
