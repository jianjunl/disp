
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

#if DISP_NAN_BOXING
struct disp_data {
#else // DISP_NAN_BOXING
union disp_data {
#endif // DISP_NAN_BOXING
    /* 闭包 / 宏 */
    struct {
        disp_val params;
        disp_val body;
        disp_scope_t *env;
        int reuse_scope;    /* 1: 可复用调用时的作用域（优化尾递归） */
    } closure;
};

#if DISP_NAN_BOXING
GC_STRUCT_TI(disp_data,
#else // DISP_NAN_BOXING
GC_UNION_TI(disp_data,
#endif // DISP_NAN_BOXING
    GC_OFF(disp_data, closure.params),
    GC_OFF(disp_data, closure.body),
    GC_OFF(disp_data, closure.env)
);

static void intern_params(disp_scope_t *env, disp_val params) {
    for (disp_val p = params; NN(p) && T(p) == FLAG_CONS; p = disp_cdr(p)) {
        disp_val sym = disp_car(p);
        if (T(sym) == FLAG_SYMBOL) {
            const char *name = disp_get_symbol_name(sym);
            if (NN(disp_find_symbol(env, name))) {
                disp_define_symbol(env, name, NIL, 0);
            }
        }
    }
}

disp_val disp_make_closure(disp_scope_t *env, disp_val params, disp_val body, int reuse_scope) {
    intern_params(env, params);
    disp_val v = ALLOC_TI(FLAG_CLOSURE, 0);
    D(v)->closure.params = params;
    D(v)->closure.body   = body;
    D(v)->closure.env    = env;
    D(v)->closure.reuse_scope = reuse_scope;
    return v;
}

disp_val disp_make_macro(disp_scope_t *env, disp_val params, disp_val body, int reuse_scope) {
    intern_params(env, params);
    disp_val v = ALLOC_TI(FLAG_MACRO, 0);
    D(v)->closure.params = params;
    D(v)->closure.body   = body;
    D(v)->closure.env    = env;
    D(v)->closure.reuse_scope = reuse_scope;
    return v;
}

disp_val disp_get_closure_params(disp_val closure) {
    if (T(closure) != FLAG_CLOSURE && T(closure) != FLAG_MACRO) {
        ERRO("disp_get_closure_params: not a closure/macro\n");
        return NIL;
    }
    return D(closure)->closure.params;
}

disp_val disp_get_closure_body(disp_val closure) {
    if (T(closure) != FLAG_CLOSURE && T(closure) != FLAG_MACRO) {
        ERRO("disp_get_closure_body: not a closure/macro\n");
        return NIL;
    }
    return D(closure)->closure.body;
}

disp_scope_t* disp_get_closure_env(disp_val closure) {
    if (T(closure) != FLAG_CLOSURE && T(closure) != FLAG_MACRO) {
        ERRO("disp_get_closure_env: not a closure/macro\n");
        return NULL;
    }
    return D(closure)->closure.env;
}

void bind_arguments_to_scope(disp_scope_t *scope, disp_val params, disp_val *args, int arg_count) {
    int fixed = 0;
    disp_val rest_sym = NIL;

    // 遍历参数列表，计算固定参数个数，并找到可能的 rest 参数
    disp_val p = params;
    while (NN(p) && T(p) == FLAG_CONS) {
        fixed++;
        p = disp_cdr(p);
    }
    if (NE(p, NIL)) {
        if (T(p) == FLAG_SYMBOL) {
            rest_sym = p;
        } else {
            // 非法 rest 参数：打印详细信息以便调试
            ERRO("bind_arguments_to_scope: rest parameter is not a symbol, got type %d", T(p));
            // 可以在这里打印 params 的表示（若有 disp_to_string 函数）
            // 然后直接返回，不执行绑定，避免后续段错误
            return;
        }
    }

    // 绑定固定参数
    int idx = 0;
    p = params;
    while (NN(p) && T(p) == FLAG_CONS && idx < fixed) {
        disp_val sym = disp_car(p);
        if (T(sym) != FLAG_SYMBOL) {
            ERRO("bind_arguments_to_scope: parameter is not a symbol");
            return;
        }
        const char *name = disp_get_symbol_name(sym);
        disp_val val = (idx < arg_count) ? args[idx] : NIL;
        disp_define_symbol(scope, name, val, 0);
        idx++;
        p = disp_cdr(p);
    }

    // 绑定 rest 参数（如果有）
    if (NE(rest_sym, NIL)) {
        const char *rest_name = disp_get_symbol_name(rest_sym);
        disp_val rest_list = NIL;
        for (int j = arg_count - 1; j >= fixed; j--) {
            rest_list = disp_make_cons(args[j], rest_list);
        }
        disp_define_symbol(scope, rest_name, rest_list, 0);
    }
}

#include "tail.h"

disp_val disp_apply_closure(disp_val closure, disp_val *args, int arg_count) {
    if (!D(closure)->closure.reuse_scope) {
        GC_ROOT(disp_scope_t, new_scope) = disp_new_scope(D(closure)->closure.env);
        bind_arguments_to_scope(new_scope, D(closure)->closure.params, args, arg_count);
        disp_val ret = disp_eval_body(new_scope, D(closure)->closure.body);
        return ret;
    }

    disp_scope_t *env = D(closure)->closure.env;
    disp_val params = D(closure)->closure.params;
    disp_val body = D(closure)->closure.body;
    disp_val *current_args = args;
    int current_argc = arg_count;
    eval_result_t res;

    // 用于保护当前参数数组的根指针（静态或局部静态，但需要线程安全）
    static _Thread_local disp_val *protected_args = NULL;  // 注意：多线程下需要 TLS 或锁

    // 保护初始参数数组（如果非空）
    if (current_args != NULL) {
        gc_add_root(&protected_args);
        protected_args = current_args;
    }

    while (1) {
        bind_arguments_to_scope(env, params, current_args, current_argc);

        disp_val exprs = body;
        while (NN(exprs) && T(exprs) == FLAG_CONS) {
            disp_val expr = disp_car(exprs);
            disp_val next = disp_cdr(exprs);
            int tail = E(next, NIL);
            res = disp_eval_tail(env, expr, tail, closure);
            // 保护新的 res
            if (res.kind == 1) {
                // 释放旧的参数数组前，先移除其根
                if (current_args != args && current_args != NULL) {
                    if (protected_args) gc_remove_root(&protected_args);
                    protected_args = NULL;
                    gc_free(current_args);
                }

                // 取出新目标和新参数
                closure = res.tail.target;
                current_args = res.tail.new_args;
                current_argc = res.tail.arg_count;

                // 保护新数组
                if (current_args != NULL) {
                    if (protected_args) gc_remove_root(&protected_args);
                    protected_args = current_args;
                    gc_add_root(&protected_args);
                }

                // 更新循环所需的新闭包内部数据
                if (T(closure) == FLAG_CLOSURE) {
                    env = D(closure)->closure.env;
                    params = D(closure)->closure.params;
                    body = D(closure)->closure.body;
                    goto restart;
                } 
                else if (T(closure) == FLAG_BUILTIN) {
                    // 内置函数：调用后直接返回（不继续蹦床）
                    disp_val result = disp_apply_builtin_from_array(closure, env, current_args, current_argc);
                    // 清理当前参数数组（如果是从堆分配的）
                    if (current_args != args && current_args != NULL)
                        gc_free(current_args);
                    return result;
                }
                else if (T(closure) == FLAG_SYSCALL) {
                    // 系统调用：参数数组直接可用
                    disp_val result = disp_get_syscall(closure)(current_args, current_argc);
                    if (current_args != args && current_args != NULL)
                        gc_free(current_args);
                    return result;
                }
                else {
                    // 非法目标
                    ERET(NIL, "tail call target is not callable");
                }
            } else if (tail) {
                disp_val result = res.normal;
                if (current_args != args && current_args != NULL) {
                    if (protected_args) {
                        gc_remove_root(&protected_args);
                        protected_args = NULL;
                    }
                    gc_free(current_args);
                }
                return result;
            } else {
                // 非尾表达式，继续下一个
                exprs = next;
            }
        }
        // body 为空时（理论上不应发生），返回 NIL
        if (current_args != args && current_args != NULL) {
            if (protected_args) {
                gc_remove_root(&protected_args);
                protected_args = NULL;
            }
            gc_free(current_args);
        }
        return NIL;

        restart:
        continue;
    }
    // 清理根（实际可能永远到不了）
    if (protected_args) gc_remove_root(&protected_args);
    return NIL;
}
