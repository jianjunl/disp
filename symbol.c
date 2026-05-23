
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

union disp_data {
    struct {
//        char *name;
        uint64_t id;            // 改为 ID，不再存储字符串指针
        disp_val *value;
    } symbol;
};

GC_UNION_TI(disp_data,
//    GC_OFF(disp_data, symbol.name),
    GC_OFF(disp_data, symbol.value)
);

/* ======================== 符号表 ======================== */
#define SYM_TABLE_SIZE 1024

void disp_set_symbol_value(disp_val *sym, disp_val *value) {
    if (!sym || T(sym) != DISP_SYMBOL) {
        ERRO("disp_set_symbol_value: not a symbol");
        return;
    }
    GC_ASSIGN_PTR(sym->data->symbol.value, value);
}

disp_val* disp_make_symbol(const char *name) {
    disp_val *v = DISP_ALLOC_TI(DISP_SYMBOL);
    if (!v) return NULL;
    
    uint64_t id = disp_get_id(name);   // 获取或创建 ID
    v->data->symbol.id = id;
    GC_ASSIGN_PTR(v->data->symbol.value, NIL);
    
    return v;
}

char* disp_get_symbol_name(disp_val *v) {
    if (!v || v->flag != DISP_SYMBOL) {
        ERRO("disp_get_symbol_name failed");
        return NULL;
    }
    uint64_t id = v->data->symbol.id;
    return (char*)disp_get_name(id);   // 通过 ID 反查字符串
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
