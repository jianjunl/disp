
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

struct disp_data {
    disp_val value;
    disp_sid id;
};

GC_STRUCT_TI(disp_data,
    GC_OFF(disp_data, value)
);

void disp_set_symbol_value_unlock(const disp_env_t *env, disp_val sym, disp_val value) {
    if (N(sym) || T(sym) != FLAG_SYMBOL) {
        ERRO("disp_set_symbol_value_unlock: not a symbol");
        return;
    }
    D(sym)->value = value;
    if (!disp_update_symbol(env, sym)) {
        //ERRO("disp_update_symbol failed");
    }
}

disp_val disp_make_symbol(disp_sid id) {
    disp_val v = ALLOC_TI(FLAG_SYMBOL, 0);
    if (N(v)) return DNULL;
    
    D(v)->id = id;
    D(v)->value = NIL;
    
    return v;
}

inline disp_val disp_make_symbol_by_name(const char *name) {
    return disp_make_symbol(disp_get_sid(name));
}

disp_sid disp_get_symbol_id(disp_val v) {
    if (N(v) || T(v) != FLAG_SYMBOL) {
        ERRO("disp_get_symbol_id failed");
        return SNULL;
    }
    return D(v)->id;
}

char* disp_get_symbol_name(disp_val v) {
    if (N(v) || T(v) != FLAG_SYMBOL) {
        ERRO("disp_get_symbol_name failed");
        return NULL;
    }
    return (char*)disp_get_name(disp_get_symbol_id(v).id);   // 通过 ID 反查字符串
}

disp_val disp_get_symbol_value(disp_val v) {
    if (N(v) || T(v) != FLAG_SYMBOL) {
        ERRO("disp_get_symbol_value failed");
    }
    return D(v)->value;
}

/* ======================== GC 初始化和全局常量 ======================== */

void disp_init_symbol() {
    NIL  = V(FLAG_VOID, 0, calloc(1, sizeof(disp_data)));
    NaN  = V(FLAG_VOID, 0, calloc(1, sizeof(disp_data)));
    TRUE = V(FLAG_VOID, 0, calloc(1, sizeof(disp_data)));
    QUIT = V(FLAG_VOID, 0, calloc(1, sizeof(disp_data)));
    DEF("false", NIL,  1);
    DEF("true",  TRUE, 1);
    DEF("quit",  QUIT, 1);
    DEF("q",     QUIT, 1);
    DEF(":q",    QUIT, 1);
}
