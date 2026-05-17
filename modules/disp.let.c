
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
extern void bind_arguments_to_scope(disp_scope_t *scope, disp_val *params, disp_val **args, int arg_count);

static disp_val* let_builtin(disp_scope_t *scope, disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "let: missing binding list");

    disp_val *first = disp_car(rest);
    if (T(first) == DISP_SYMBOL) {
        disp_val *second_rest = disp_cdr(rest);
        if (second_rest && T(second_rest) == DISP_CONS)
            return disp_letf(scope, expr);
    }

    disp_val *bindings = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body)
        ERET(NIL, "let: missing body");

    int var_count = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b))
        var_count++;

    if (var_count == 0)
        return disp_eval_body(scope, body);

    /* 提取变量符号和初值表达式 */
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
            ERET(NIL, "let: malformed binding (var expr)");
        }
        var_syms[idx] = disp_car(pair);
        init_exprs[idx] = disp_car(disp_cdr(pair));
        idx++;
        b = disp_cdr(b);
    }

    /* 创建新作用域，父作用域为当前 scope */
    disp_scope_t *new_scope = disp_new_scope(scope);
    gc_add_root(&new_scope);

    disp_val *result = NIL;

    if (disp_car(expr) == LETA) {   /* let* : 顺序绑定 */
        for (int i = 0; i < var_count; i++) {
            disp_val *val = disp_eval(new_scope, init_exprs[i]);
            /* 立即绑定到新作用域 */
            const char *name = disp_get_symbol_name(var_syms[i]);
            disp_define_symbol(new_scope, name, val, 0);
        }
        result = disp_eval_body(new_scope, body);
    } else {                         /* let : 并行绑定 */
        /* 在当前作用域中计算所有初值 */
        disp_val **values = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
        gc_add_root(&values);
        for (int i = 0; i < var_count; i++) {
            values[i] = disp_eval(scope, init_exprs[i]);
            gc_add_root(&values[i]);
        }
        /* 构造参数列表（符号列表） */
        disp_val *params = NIL;
        for (int i = var_count - 1; i >= 0; i--)
            params = disp_make_cons(var_syms[i], params);
        /* 一次性绑定到新作用域 */
        bind_arguments_to_scope(new_scope, params, values, var_count);
        /* 执行 body */
        result = disp_eval_body(new_scope, body);
        /* 清理 values 的保护 */
        for (int i = 0; i < var_count; i++)
            gc_remove_root(&values[i]);
        gc_remove_root(&values);
        gc_free(values);
    }

    /* 清理资源 */
    gc_remove_root(&new_scope);
    gc_remove_root(&var_syms);
    gc_remove_root(&init_exprs);
    gc_free(var_syms);
    gc_free(init_exprs);

    return result;
}

/* 模块初始化函数 */
void disp_init_module(void) {
    DEF("let"     , MKB(let_builtin    , "<#let>"    ), 1);
    DEF("let*"    , MKB(let_builtin    , "<#let*>"   ), 1);
}
