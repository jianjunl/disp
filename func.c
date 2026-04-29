
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

#ifndef DISP_ALLINONE_UNION_H
union disp_data {
    /* 内置函数 / 系统调用 */
    struct {
        disp_builtin_t func;
        char *desc;
    } builtin; // for special_form/syntax
    struct {
        disp_syscall_t func;
        char *desc;
    } syscall; // for function/primitive

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
#endif

disp_builtin_t disp_get_builtin(disp_val *v) {
    if (v->flag == DISP_BUILTIN) {
        return v->data->builtin.func;
    }
    ERET(NULL, "disp_get_builtin failed: %s\n", disp_str(v));
}

char* disp_get_builtin_desc(disp_val *v) {
    if (v->flag == DISP_BUILTIN) {
        return v->data->builtin.desc;
    }
    ERET(NULL, "disp_get_builtin_desc failed: %s\n", strerror (errno));
}

disp_syscall_t disp_get_syscall(disp_val *v) {
    if (v->flag == DISP_SYSCALL) {
        return v->data->syscall.func;
    }
    ERET(NULL, "disp_get_syscall failed: %s\n", strerror (errno));
}

char* disp_get_syscall_desc(disp_val *v) {
    if (v->flag == DISP_SYSCALL) {
        return v->data->syscall.desc;
    }
    ERET(NULL, "disp_get_syscall_desc failed: %s\n", strerror (errno));
}

disp_val* disp_make_builtin(disp_builtin_t f, char *d) {
    disp_val *v = DISP_ALLOC(DISP_BUILTIN);
    v->data->builtin.func = f;
    v->data->builtin.desc = d;
    gc_add_root(&v->data->builtin.func);
    gc_add_root(&v->data->builtin.desc);
    gc_add_root(&v->data->builtin);
    gc_add_root(&v->data);
    gc_add_root(&v);
    return v;
}

disp_val* disp_make_syscall(disp_syscall_t f, char *d) {
    disp_val *v = DISP_ALLOC(DISP_SYSCALL);
    v->data->syscall.func = f;
    v->data->syscall.desc = d;
    gc_add_root(&v->data->syscall.func);
    gc_add_root(&v->data->syscall.desc);
    gc_add_root(&v->data->syscall);
    gc_add_root(&v->data);
    gc_add_root(&v);
    return v;
}

static void intern_params(disp_val *params) {
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        disp_val *sym = disp_car(p);
        if (T(sym) == DISP_SYMBOL) {
            const char *name = disp_get_symbol_name(sym);
            if (!disp_find_symbol(name)) {
                disp_define_symbol(name, NIL, 0);
            }
        }
    }
}

disp_val* disp_make_closure(disp_val *params, disp_val *body) {
    intern_params(params);
    disp_val *v = DISP_ALLOC(DISP_CLOSURE);
    v->data->closure.params = params;
    v->data->closure.body = body;
    return v;
}

disp_val* disp_make_macro(disp_val *params, disp_val *body) {
    intern_params(params);
    disp_val *v = DISP_ALLOC(DISP_MACRO);
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
    disp_val *v = DISP_ALLOC(DISP_TYPE);
    v->data->type_val.name = name;
    v->data->type_val.decl = decl;
    return disp_define_symbol(name, v, 1);
}
