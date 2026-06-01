
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

static disp_val letrec_builtin(disp_env_t *env, disp_val expr) {
    disp_val rest = disp_cdr(expr);
    if (N(rest) || T(rest) != FLAG_CONS)
        ERET(NIL, "letrec: missing binding list");

    disp_val first = disp_car(rest);
    if (T(first) == FLAG_SYMBOL) {
        disp_val second_rest = disp_cdr(rest);
        if (NN(second_rest) && T(second_rest) == FLAG_CONS)
            return disp_letf(env, expr);
    }

    disp_val bindings = disp_car(rest);
    disp_val body = disp_cdr(rest);
    if (N(body))
        ERET(NIL, "letrec: missing body");

    int var_count = 0;
    for (disp_val b = bindings; NN(b) && T(b) == FLAG_CONS; b = disp_cdr(b))
        var_count++;

    if (var_count == 0)
        return disp_eval_body(env, body);

    GC_ROOT(disp_val, var_syms) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    GC_ROOT(disp_val, init_exprs) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);

    int idx = 0;
    disp_val b = bindings;
    while (NN(b) && T(b) == FLAG_CONS) {
        disp_val pair = disp_car(b);
        if (T(pair) != FLAG_CONS || T(disp_car(pair)) != FLAG_SYMBOL) {
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
    GC_ROOT(disp_env_t, new_env) = disp_new_env(env);

    /* 先在新作用域中绑定占位符 NIL */
    for (int i = 0; i < var_count; i++) {
        uint64_t id = SI(var_syms[i]);
        disp_define_symbol(new_env, id, NIL, 0);
    }

    /* 计算所有初值（在新作用域中，此时变量已存在但值为 NIL） */
    GC_ROOT(disp_val, values) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    if (E(disp_car(expr), LETRECA)) {   /* letrec* : 顺序初始化 */
        for (int i = 0; i < var_count; i++) {
            values[i] = disp_eval(new_env, init_exprs[i]);
            /* 立即更新绑定 */
            uint64_t id = SI(var_syms[i]);
            disp_define_symbol(new_env, id, values[i], 0);
        }
    } else {                           /* letrec : 并行初始化 */
        for (int i = 0; i < var_count; i++) {
            values[i] = disp_eval(new_env, init_exprs[i]);
        }
        for (int i = 0; i < var_count; i++) {
            uint64_t id = SI(var_syms[i]);
            disp_define_symbol(new_env, id, values[i], 0);
        }
    }

    /* 执行 body */
    disp_val result = disp_eval_body(new_env, body);

    /* 清理 */
    gc_free(values);
    gc_free(var_syms);
    gc_free(init_exprs);

    return result;
}

/* 模块初始化函数 */
void disp_init_module(void) {
    DEF("letrec"  , MKB(letrec_builtin , "<#letrec>" ), 1);
    DEF("letrec*" , MKB(letrec_builtin , "<#letrec*>"), 1);
}
