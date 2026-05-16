
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
        disp_val *name;
        disp_val *params;
        disp_val *body;
        disp_scope_t *env;
        int reuse_scope;    /* 1: 可复用调用时的作用域（优化尾递归） */
    } closure;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, closure.name),
    GC_OFF(disp_data, closure.params),
    GC_OFF(disp_data, closure.body),
    GC_OFF(disp_data, closure.env)
);

static void intern_params(disp_scope_t *env, disp_val *params) {
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        disp_val *sym = disp_car(p);
        if (T(sym) == DISP_SYMBOL) {
            const char *name = disp_get_symbol_name(sym);
            if (!disp_find_symbol(env, name)) {
                disp_define_symbol(env, name, NIL, 0);
            }
        }
    }
}

disp_val* disp_make_closure(disp_scope_t *env, disp_val *name, disp_val *params, disp_val *body, int reuse_scope) {
    intern_params(env, params);
    disp_val *v = DISP_ALLOC_TI(DISP_CLOSURE);
    v->data->closure.name   = name;
    v->data->closure.params = params;
    v->data->closure.body   = body;
    v->data->closure.env    = env;
    v->data->closure.reuse_scope = reuse_scope;
    return v;
}

disp_val* disp_make_macro(disp_scope_t *env, disp_val *name, disp_val *params, disp_val *body, int reuse_scope) {
    intern_params(env, params);
    disp_val *v = DISP_ALLOC_TI(DISP_MACRO);
    v->data->closure.name   = name;
    v->data->closure.params = params;
    v->data->closure.body   = body;
    v->data->closure.env    = env;
    v->data->closure.reuse_scope = reuse_scope;
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

disp_scope_t* disp_get_closure_env(disp_val *closure) {
    if (T(closure) != DISP_CLOSURE && T(closure) != DISP_MACRO) {
        ERRO("disp_get_closure_env: not a closure/macro\n");
        return NULL;
    }
    return closure->data->closure.env;
}

// 设置闭包的 reuse_scope 标志
void disp_set_reuse_scope(disp_val *closure) {
    if (closure && (T(closure) == DISP_CLOSURE || T(closure) == DISP_MACRO)) {
        closure->data->closure.reuse_scope = 1;
    }
}

void bind_arguments_to_scope(disp_scope_t *scope, disp_val *params, disp_val **args, int arg_count) {
    // 处理固定参数和 rest 参数（与原来类似，但直接在 scope 上 define）
    int fixed = 0;
    disp_val *rest_sym = NIL;
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) fixed++;
    if (params && T(params) != DISP_CONS) rest_sym = params;

    int idx = 0;
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        const char *name = disp_get_symbol_name(disp_car(p));
        disp_define_symbol(scope, name, args[idx++], 0);
    }
    if (rest_sym != NIL) {
        // 收集剩余参数为列表
        const char *rest_name = disp_get_symbol_name(rest_sym);
        disp_val *rest_list = NIL;
        for (int j = arg_count - 1; j >= fixed; j--) {
            rest_list = disp_make_cons(args[j], rest_list);
        }
        disp_define_symbol(scope, rest_name, rest_list, 0);
    }
}

void bind_arguments_to_scope_skip(disp_scope_t *scope, disp_val *params, disp_val **args, int arg_count, const char *skip_name) {
    int fixed = 0;
    disp_val *rest_sym = NIL;
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) fixed++;
    if (params && T(params) != DISP_CONS) rest_sym = params;

    int idx = 0;
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        disp_val *sym = disp_car(p);
        const char *name = disp_get_symbol_name(sym);
        if (skip_name && strcmp(name, skip_name) == 0) {
            idx++;  // 跳过绑定，但参数索引仍要前进
            continue;
        }
        disp_define_symbol(scope, name, args[idx++], 0);
    }
    if (rest_sym != NIL) {
        const char *rest_name = disp_get_symbol_name(rest_sym);
        if (!(skip_name && strcmp(rest_name, skip_name) == 0)) {
            disp_val *rest_list = NIL;
            for (int j = arg_count - 1; j >= fixed; j--) {
                rest_list = disp_make_cons(args[j], rest_list);
            }
            disp_define_symbol(scope, rest_name, rest_list, 0);
        }
    }
}

disp_val* disp_apply_closure(disp_val *closure, disp_val **args, int arg_count) {
    disp_val *params = disp_get_closure_params(closure);
    disp_val *body   = disp_get_closure_body(closure);
    disp_scope_t *env = disp_get_closure_env(closure);
    disp_val *result = NIL;
    /* 如果闭包不允许复用作用域，则按标准方式：创建新作用域，执行一次 body 后返回 */
    if (!closure->data->closure.reuse_scope) {
        disp_scope_t *new_scope = disp_new_scope(env);
        bind_arguments_to_scope(new_scope, params, args, arg_count);
        return disp_eval_body(new_scope, body);
    }
    /* 支持尾递归优化的循环版本 */
    const char *closure_name = NULL;
    if (closure->data->closure.name && T(closure->data->closure.name) == DISP_SYMBOL) {
        closure_name = disp_get_symbol_name(closure->data->closure.name);
    }
    while (1) {
        /* 1. 将实参绑定到当前环境（更新已存在的符号值） */
        /* 绑定参数时跳过闭包自身的名字 */
        bind_arguments_to_scope_skip(env, params, args, arg_count, closure_name);


        /* 2. 顺序执行 body 中的表达式，遇到自调用则立即重新开始 */
        disp_val *exprs = body;
        while (exprs && T(exprs) == DISP_CONS) {
            disp_val *expr = disp_car(exprs);
            disp_val *next = disp_cdr(exprs);

            /* 检测自调用（无论位置） */
            if (T(expr) == DISP_CONS) {
                disp_val *func = disp_car(expr);
                //disp_val *func_expr = disp_car(expr);
                //disp_val *func = disp_eval(env, func_expr);
fprintf(stderr, "DEBUG: func=%p, closure=%p\n", func, closure);
fprintf(stderr, "DEBUG: T(func)=%d, T(closure)=%d\n", T(func), T(closure));
fprintf(stderr, "DEBUG: func == closure ? %d\n", func == closure);
                if (T(func) == DISP_CLOSURE && func == closure) {
                    /* 尾递归调用：提取新参数并求值 */
                    disp_val *arg_list = disp_cdr(expr);
                    int new_argc = 0;
                    for (disp_val *a = arg_list; a && T(a) == DISP_CONS; a = disp_cdr(a))
                        new_argc++;
                    if (new_argc != arg_count) {
                        ERET(NIL, "tail recursion: arity mismatch (%d vs %d)", new_argc, arg_count);
                    }
                    int i = 0;
                    for (disp_val *a = arg_list; a && T(a) == DISP_CONS; a = disp_cdr(a)) {
                        args[i++] = disp_eval(env, disp_car(a));
                    }
                    /* 跳过当前及后续所有表达式，重新开始循环 */
                    goto restart;
                }
            }

            /* 非自调用：正常求值 */
            if (next == NIL) {
                result = disp_eval(env, expr);
                return result;
            } else {
                disp_eval(env, expr);
                exprs = next;
            }
        }
        restart:
        continue;
    }
}
