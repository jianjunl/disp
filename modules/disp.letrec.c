
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

extern disp_val* disp_letf(disp_scope_t *scope, disp_val *expr);

static disp_val* letrec_builtin(disp_scope_t *scope, disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "letrec: missing binding list");

    disp_val *first = disp_car(rest);
    if (T(first) == DISP_SYMBOL) {
        disp_val *second_rest = disp_cdr(rest);
        if (second_rest && T(second_rest) == DISP_CONS)
            return disp_letf(scope, expr);
    }

    disp_val *bindings = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body)
        ERET(NIL, "letrec: missing body");

    int var_count = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b))
        var_count++;

    if (var_count == 0)
        return disp_eval_body(scope, body);

    disp_val **var_syms = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    disp_val **init_exprs = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    gc_add_root(&var_syms);
    gc_add_root(&init_exprs);

    int idx = 0;
    disp_val *b = bindings;
    while (b && T(b) == DISP_CONS) {
        disp_val *pair = disp_car(b);
        if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
            gc_remove_root(&init_exprs);
            gc_remove_root(&var_syms);
            gc_free(init_exprs);
            gc_free(var_syms);
            ERET(NIL, "letrec: malformed binding (var expr)");
        }
        var_syms[idx] = disp_car(pair);
        init_exprs[idx] = disp_car(disp_cdr(pair));
        idx++;
        b = disp_cdr(b);
    }

    /* 创建新作用域 */
    GC_ROOT(disp_scope_t, new_scope) = disp_new_scope(scope);

    /* 先在新作用域中绑定占位符 NIL */
    for (int i = 0; i < var_count; i++) {
        const char *name = disp_get_symbol_name(var_syms[i]);
        disp_define_symbol(new_scope, name, NIL, 0);
    }

    /* 计算所有初值（在新作用域中，此时变量已存在但值为 NIL） */
    disp_val **values = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    gc_add_root(&values);
    if (disp_car(expr) == LETRECA) {   /* letrec* : 顺序初始化 */
        for (int i = 0; i < var_count; i++) {
            values[i] = disp_eval(new_scope, init_exprs[i]);
            gc_add_root(&values[i]);
            /* 立即更新绑定 */
            const char *name = disp_get_symbol_name(var_syms[i]);
            disp_define_symbol(new_scope, name, values[i], 0);
        }
    } else {                           /* letrec : 并行初始化 */
        for (int i = 0; i < var_count; i++) {
            values[i] = disp_eval(new_scope, init_exprs[i]);
            gc_add_root(&values[i]);
        }
        for (int i = 0; i < var_count; i++) {
            const char *name = disp_get_symbol_name(var_syms[i]);
            disp_define_symbol(new_scope, name, values[i], 0);
        }
    }

    /* 执行 body */
    disp_val *result = disp_eval_body(new_scope, body);

    /* 清理 */
    for (int i = 0; i < var_count; i++)
        gc_remove_root(&values[i]);
    gc_remove_root(&values);
    gc_free(values);
    gc_remove_root(&var_syms);
    gc_remove_root(&init_exprs);
    gc_free(var_syms);
    gc_free(init_exprs);

    return result;
}

/* 模块初始化函数 */
void disp_init_module(void) {
    DEF("letrec"  , MKB(letrec_builtin , "<#letrec>" ), 1);
    DEF("letrec*" , MKB(letrec_builtin , "<#letrec*>"), 1);
}
