
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

#if DISP_NAN_BOXING
struct disp_data {
    disp_flag_t tag;
    struct {
        uint64_t id;
        disp_val value;
    } symbol;
};

GC_STRUCT_TI(disp_data,
    GC_OFF(disp_data, symbol.value)
);

#else // DISP_NAN_BOXING

union disp_data {
    struct {
        uint64_t id;
        disp_val value;
    } symbol;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, symbol.value)
);

#endif

/* ======================== 符号表 ======================== */
#define SYM_TABLE_SIZE 1024

void disp_set_symbol_value(disp_val sym, disp_val value) {
    if (N(sym) || T(sym) != FLAG_SYMBOL) {
        ERRO("disp_set_symbol_value: not a symbol");
        return;
    }
    D(sym)->symbol.value = value;
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
    NIL  = V(FLAG_VOID, 0, calloc(1, sizeof(disp_data)));
    TRUE = V(FLAG_VOID, 0, calloc(1, sizeof(disp_data)));
    QUIT = V(FLAG_VOID, 0, calloc(1, sizeof(disp_data)));
    DEF("nil",   NIL,  1);
    DEF("false", NIL,  1);
    DEF("true",  TRUE, 1);
    DEF("quit",  QUIT, 1);
    DEF("q",     QUIT, 1);
    DEF(":q",    QUIT, 1);
}
