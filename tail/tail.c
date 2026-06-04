
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
#include "../disp.h"

#include "tail.h"

// 辅助：判断表达式是否为自求值原子
static int is_self_evaluating(disp_val expr) {
    int t = T(expr);
    return (t == FLAG_BYTE || t == FLAG_SHORT || t == FLAG_INT || t == FLAG_LONG || t == TAG_LONG
            || t == FLAG_FLOAT || t == FLAG_DOUBLE || t == FLAG_STRING
            || E(expr, TRUE) || E(expr, NIL) || E(expr, QUIT));
}

// 辅助：求值参数列表（用于自调用）
static disp_val * eval_args_for_tail(disp_env_t *env, disp_val arg_list, int *arg_count) {
    *arg_count = 0;
    for (disp_val a = arg_list; NN(a) && T(a) == FLAG_CONS; a = disp_cdr(a))
        (*arg_count)++;
    if (*arg_count == 0) return NULL;
    GC_ROOT(disp_val, args) = gc_typed_malloc(*arg_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    int i = 0;
    for (disp_val a = arg_list; NN(a) && T(a) == FLAG_CONS; a = disp_cdr(a)) {
        args[i++] = disp_eval(env, disp_car(a));
    }
    return args;
}

// 核心尾位置求值函数
eval_result_t disp_eval_tail(disp_env_t *env, disp_val expr, int is_tail, disp_val current_closure) {
    if (E(expr, NIL)) return RESULT_NORMAL(NIL);

    // 自求值原子
    if (is_self_evaluating(expr)) {
        return RESULT_NORMAL(expr);
    }

    // 符号：查找变量
    if (T(expr) == FLAG_SYMBOL) {
        disp_val sym = disp_find_symbol(env, SYM_ID(expr));
        if (N(sym)) {
            ERRO("unbound symbol: %s", SYM_NAME(expr));
            return RESULT_NORMAL(NIL);
        }
        return RESULT_NORMAL(SYM_VALUE(sym));
    }

    // 不是 cons 则错误
    if (T(expr) != FLAG_CONS) {
        ERRO("invalid expression");
        return RESULT_NORMAL(NIL);
    }

    disp_val op = disp_car(expr);
    disp_val args = disp_cdr(expr);

    // 特殊形式处理
    if (T(op) == FLAG_SYMBOL) {
        if (SYM_ID(op) == QUOTE)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == IF)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == BEGIN || SYM_ID(op) == PROGN)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == COND)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == AND)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == OR)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == SET || SYM_ID(op) == SETQ)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == DEFINE)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == LAMBDA)
            return disp_eval_tail_flow(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == LET)
            return disp_eval_tail_let(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == LETA)
            return disp_eval_tail_leta(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == LETREC)
            return disp_eval_tail_letrec(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == LETRECA)
            return disp_eval_tail_letreca(env, expr, is_tail, current_closure);
        if (SYM_ID(op) == DO || SYM_ID(op) == DOTIMES || SYM_ID(op) == DOLIST) {
            disp_val func = disp_eval(env, op);
            if (T(func) == FLAG_BUILTIN) {
                disp_val result = disp_get_builtin(func)(env, expr);
                return RESULT_NORMAL(result);
            } else {
                ERRO("%s is not a builtin", SYM_NAME(op));
                return RESULT_NORMAL(NIL);
            }
        }
    }

    // 函数调用
    // 先求值操作符
    disp_val func = disp_eval(env, op);

    if (is_tail && (T(func) == FLAG_CLOSURE || T(func) == FLAG_BUILTIN || T(func) == FLAG_SYSCALL)) {
        // 尾调用优化：求值参数后返回尾调用结果（不立即应用）
        int arg_count = 0;
        disp_val *args_arr = eval_args_for_tail(env, args, &arg_count);
        GC_ROOT_AUTO(args_arr);
        return RESULT_TAIL(func, args_arr, arg_count);
    } else {
        // 正常调用：求值参数并应用
        disp_val result;
        if (T(func) == FLAG_BUILTIN) {
            result = disp_get_builtin(func)(env, expr);
            return RESULT_NORMAL(result);
        } else {
            int arg_count = 0;
            for (disp_val a = args; NN(a) && T(a) == FLAG_CONS; a = disp_cdr(a)) arg_count++;
            GC_ROOT(disp_val, argv) = gc_typed_malloc(arg_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
            int i = 0;
            for (disp_val a = args; NN(a) && T(a) == FLAG_CONS; a = disp_cdr(a)) {
                argv[i++] = disp_eval(env, disp_car(a));
            }
            if (T(func) == FLAG_CLOSURE) {
                result = disp_apply_closure(func, argv, arg_count);
            } else if (T(func) == FLAG_SYSCALL) {
                result = disp_get_syscall(func)(argv, arg_count);
            } else {
                gc_free(argv);
                if (T(func) != FLAG_CLOSURE && T(func) != FLAG_BUILTIN && T(func) != FLAG_SYSCALL) {
                    ERRO("func type=%d, value=%p, symbol name=%s\n", T(func), D(func), SYM_NAME(func));
                }
                char *s = disp_string(func);
                ERRO("%s is not a function or macro", s);
                return RESULT_NORMAL(NIL);
            }
            gc_free(argv);
            return RESULT_NORMAL(result);
        }
    }
}
