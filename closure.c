
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

/* ======================== Funcs ======================== */

struct disp_data {
    /* 闭包 / 宏 */
    disp_val params;
    disp_val body;
    disp_env_t *env;
    int reuse_env;    /* 1: 可复用调用时的作用域（优化尾递归） */
};

GC_STRUCT_TI(disp_data,
    GC_OFF(disp_data, params),
    GC_OFF(disp_data, body),
    GC_OFF(disp_data, env)
);

disp_val disp_make_closure(disp_env_t *env, disp_val params, disp_val body, int reuse_env) {
    disp_val v = ALLOC_TI(FLAG_CLOSURE, 0);
    D(v)->params = params;
    D(v)->body   = body;
    D(v)->env    = env;
    D(v)->reuse_env = reuse_env;
    return v;
}

disp_val disp_make_macro(disp_env_t *env, disp_val params, disp_val body, int reuse_env) {
    disp_val v = ALLOC_TI(FLAG_MACRO, 0);
    D(v)->params = params;
    D(v)->body   = body;
    D(v)->env    = env;
    D(v)->reuse_env = reuse_env;
    return v;
}

disp_val disp_get_closure_params(disp_val closure) {
    if (T(closure) != FLAG_CLOSURE && T(closure) != FLAG_MACRO) {
        ERRO("disp_get_closure_params: not a closure/macro\n");
        return NIL;
    }
    return D(closure)->params;
}

disp_val disp_get_closure_body(disp_val closure) {
    if (T(closure) != FLAG_CLOSURE && T(closure) != FLAG_MACRO) {
        ERRO("disp_get_closure_body: not a closure/macro\n");
        return NIL;
    }
    return D(closure)->body;
}

disp_env_t* disp_get_closure_env(disp_val closure) {
    if (T(closure) != FLAG_CLOSURE && T(closure) != FLAG_MACRO) {
        ERRO("disp_get_closure_env: not a closure/macro\n");
        return NULL;
    }
    return D(closure)->env;
}

int disp_is_closure_reuse_env(disp_val closure) {
    if (T(closure) != FLAG_CLOSURE && T(closure) != FLAG_MACRO) {
        ERRO("disp_is_closure_reuse_env: not a closure/macro\n");
        return 0;
    }
    return D(closure)->reuse_env;
}
