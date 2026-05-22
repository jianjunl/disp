
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

#include "tail.h"

// 辅助：判断表达式是否为自求值原子
static int is_self_evaluating(disp_val *expr) {
    int t = T(expr);
    return (t == DISP_BYTE || t == DISP_SHORT || t == DISP_INT || t == DISP_LONG
            || t == DISP_FLOAT || t == DISP_DOUBLE || t == DISP_STRING
            || expr == TRUE || expr == NIL || expr == QUIT);
}

// 辅助：求值参数列表（用于自调用）
static disp_val** eval_args_for_tail(disp_scope_t *env, disp_val *arg_list, int *arg_count) {
    *arg_count = 0;
    for (disp_val *a = arg_list; a && T(a) == DISP_CONS; a = disp_cdr(a))
        (*arg_count)++;
    if (*arg_count == 0) return NULL;
    GC_ROOT(disp_val*, args) = gc_typed_malloc(*arg_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    int i = 0;
    for (disp_val *a = arg_list; a && T(a) == DISP_CONS; a = disp_cdr(a)) {
        args[i++] = disp_eval(env, disp_car(a));
    }
    return args;
}

eval_result_t* result_nil() {
    static eval_result_t ret  = (eval_result_t){.kind = 0, .normal = NULL};
    if(!ret.normal) ret.normal = NIL;
    return &ret;
}

eval_result_t* result_true() {
    static eval_result_t ret  = (eval_result_t){.kind = 0, .normal = NULL};
    if(!ret.normal) ret.normal = TRUE;
    return &ret;
}

GC_TYPE_INFO(eval_result_ti, eval_result_t,
    offsetof(eval_result_t, normal),       // disp_val*
    offsetof(eval_result_t, tail.target),  // disp_val*
    offsetof(eval_result_t, tail.new_args) // disp_val**
);

eval_result_t* result_normal(disp_val *normal) {
    GC_ROOT(eval_result_t, res) = gc_typed_malloc(sizeof(eval_result_t), &eval_result_ti);
    res->kind = 0;
    res->normal = normal;
    return res;
}

static eval_result_t* result_tail(disp_val *target, disp_val **new_argv, int new_argc) {
    GC_ROOT(eval_result_t, res) = gc_typed_malloc(sizeof(eval_result_t), &eval_result_ti);
    res->kind = 1;
    res->tail.target = target;
    res->tail.new_args = new_argv;
    res->tail.arg_count = new_argc;
    return res;
}

// 核心尾位置求值函数
eval_result_t* disp_eval_tail(disp_scope_t *env, disp_val *expr, int is_tail, disp_val *current_closure) {
    if (expr == NIL) return result_nil();

    // 自求值原子
    if (is_self_evaluating(expr)) {
        return result_normal(expr);
    }

    // 符号：查找变量
    if (T(expr) == DISP_SYMBOL) {
        disp_val *sym = disp_find_symbol(env, disp_get_symbol_name(expr));
        if (!sym) {
            ERRO("unbound symbol: %s", disp_get_symbol_name(expr));
            return result_nil();
        }
        return result_normal(disp_get_symbol_value(sym));
    }

    // 不是 cons 则错误
    if (T(expr) != DISP_CONS) {
        ERRO("invalid expression");
        return result_nil();
    }

    disp_val *op = disp_car(expr);
    disp_val *args = disp_cdr(expr);

    // 特殊形式处理
    if (T(op) == DISP_SYMBOL) {
        if (op == QUOTE)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (op == IF)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (op == BEGIN || op == PROGN)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (op == COND)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (op == AND)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (op == OR)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (op == SET || op == SETQ)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (op == DEFINE)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (op == LAMBDA)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (op == LET)
            return disp_eval_tail_let(env, expr, is_tail, current_closure);
        if (op == LETA)
            return disp_eval_tail_leta(env, expr, is_tail, current_closure);
        if (op == LETREC)
            return disp_eval_tail_letrec(env, expr, is_tail, current_closure);
        if (op == LETRECA)
            return disp_eval_tail_letreca(env, expr, is_tail, current_closure);
    }

    // 函数调用
    // 先求值操作符
    disp_val *func = disp_eval(env, op);

    if (is_tail && (T(func) == DISP_CLOSURE || T(func) == DISP_BUILTIN || T(func) == DISP_SYSCALL)) {
        // 尾调用优化：求值参数后返回尾调用结果（不立即应用）
        int arg_count = 0;
        disp_val **args_arr = eval_args_for_tail(env, args, &arg_count);
        return result_tail(func, args_arr, arg_count);
    } else {
        // 正常调用：求值参数并应用
        disp_val *result;
        if (T(func) == DISP_BUILTIN) {
            result = disp_get_builtin(func)(env, expr);
            return result_normal(result);
        } else {
            int arg_count = 0;
            for (disp_val *a = args; a && T(a) == DISP_CONS; a = disp_cdr(a)) arg_count++;
            GC_ROOT(disp_val*, argv) = gc_typed_malloc(arg_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
            int i = 0;
            for (disp_val *a = args; a && T(a) == DISP_CONS; a = disp_cdr(a)) {
                argv[i++] = disp_eval(env, disp_car(a));
            }
            if (T(func) == DISP_CLOSURE) {
                result = disp_apply_closure(func, argv, arg_count);
            } else if (T(func) == DISP_SYSCALL) {
                result = disp_get_syscall(func)(argv, arg_count);
            } else {
                gc_free(argv);
                if (T(func) != DISP_CLOSURE && T(func) != DISP_BUILTIN && T(func) != DISP_SYSCALL) {
                    ERRO("func type=%d, value=%p, symbol name=%s\n", T(func), func, disp_get_symbol_name(func));
                }
                char *s = disp_string(func);
                ERRO("%s is not a function or macro", s);
                return result_nil();
            }
            gc_free(argv);
            return result_normal(result);
        }
    }
}

disp_val* disp_apply_builtin_from_array(disp_val *builtin, disp_scope_t *env, disp_val **args, int arg_count) {
    // 1. 构建参数列表 (arg1 arg2 ... argN)
    disp_val *arg_list = NIL;
    for (int i = arg_count - 1; i >= 0; i--) {
        arg_list = disp_make_cons(args[i], arg_list);
    }
    // 2. 构建完整调用表达式 (builtin arg1 arg2 ...)
    disp_val *expr = disp_make_cons(builtin, arg_list);
    // 3. 调用内置函数
    disp_val *result = disp_get_builtin(builtin)(env, expr);
    // 临时构造的 cons 节点会在下次 GC 时自动回收（无根引用）
    return result;
}
