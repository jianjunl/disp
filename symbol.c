
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
        uint64_t id;
        disp_val value;
    } symbol;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, symbol.value)
);

/* ======================== 符号表 ======================== */
#define SYM_TABLE_SIZE 1024

void disp_set_symbol_value(disp_val sym, disp_val value) {
    if (N(sym) || T(sym) != FLAG_SYMBOL) {
        ERRO("disp_set_symbol_value: not a symbol");
        return;
    }
    GC_ASSIGN_PTR(sym.data->symbol.value, value);
}
disp_val disp_make_symbol(const char *name) {
    disp_val v = ALLOC_TI(FLAG_SYMBOL, 0);
    if (N(v)) return DNULL;
    
    uint64_t id = disp_get_id(name);   // 获取或创建 ID
    D(v)->symbol.id = id;
    D(v)->symbol.value = NIL;
    
    return v;
}

char* disp_get_symbol_name(disp_val v) {
    if (N(v) || T(v) != FLAG_SYMBOL) {
        ERRO("disp_get_symbol_name failed");
        return NULL;
    }
    uint64_t id = D(v)->symbol.id;
    return (char*)disp_get_name(id);   // 通过 ID 反查字符串
}

disp_val disp_get_symbol_value(disp_val v) {
    if (N(v) || T(v) != FLAG_SYMBOL) {
        ERRO("disp_get_symbol_value failed");
    }
    return D(v)->symbol.value;
}

/* ======================== GC 初始化和全局常量 ======================== */

void disp_init_symbol() {
    NIL  = ALLOC(FLAG_VOID, 0);
    TRUE = ALLOC(FLAG_VOID, 0);
    QUIT = ALLOC(FLAG_VOID, 0);
    DEF("nil",   NIL,  1);
    DEF("false", NIL,  1);
    DEF("true",  TRUE, 1);
    DEF("quit",  QUIT, 1);
    DEF("q",     QUIT, 1);
    DEF(":q",    QUIT, 1);
}
