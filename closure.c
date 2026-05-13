
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

union disp_data {
    /* 闭包 / 宏 */
    struct {
        disp_val *params;
        disp_val *body;
    } closure;

    /* 类型 */
    struct {
        char *name;
        disp_val *decl;
    } type_val;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, closure.params),
    GC_OFF(disp_data, closure.body)
);

static void intern_params(disp_val *params) {
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        disp_val *sym = disp_car(p);
        if (T(sym) == DISP_SYMBOL) {
            const char *name = disp_get_symbol_name(sym);
            if (!disp_find_symbol(NULL, name)) {
                disp_define_symbol(NULL, name, NIL, 0);
            }
        }
    }
}

disp_val* disp_make_closure(disp_val *params, disp_val *body) {
    intern_params(params);
    disp_val *v = DISP_ALLOC_TI(DISP_CLOSURE);
    v->data->closure.params = params;
    v->data->closure.body = body;
    return v;
}

disp_val* disp_make_macro(disp_val *params, disp_val *body) {
    intern_params(params);
    disp_val *v = DISP_ALLOC_TI(DISP_MACRO);
    v->data->closure.params = params;
    v->data->closure.body = body;
    return v;
}

disp_val* disp_get_closure_params(disp_val *closure) {
    if (T(closure) != DISP_CLOSURE && T(closure) != DISP_MACRO) {
        ERRO("disp_get_closure_params: not a closure/macro\n");
        return NIL;
    }
    return closure->data->closure.params;
}

disp_val* disp_get_closure_body(disp_val *closure) {
    if (T(closure) != DISP_CLOSURE && T(closure) != DISP_MACRO) {
        ERRO("disp_get_closure_body: not a closure/macro\n");
        return NIL;
    }
    return closure->data->closure.body;
}

char* disp_get_type_name(disp_val *v) {
    if (v->flag != DISP_TYPE) {
	ERRO("disp_get_type_name failed: %s\n", strerror (errno));
    }
    return v->data->type_val.name;
}

disp_val* disp_get_type_decl(disp_val *v) {
    if (v->flag != DISP_TYPE) {
	ERRO("T_decl failed: %s\n", strerror (errno));
    }
    return v->data->type_val.decl;
}

disp_val* disp_define_type(char *name, disp_val *decl) {
    disp_val *v = DISP_ALLOC_TI(DISP_TYPE);
    v->data->type_val.name = name;
    v->data->type_val.decl = decl;
    return disp_define_symbol(NULL, name, v, 1);
}
