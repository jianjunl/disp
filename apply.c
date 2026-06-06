
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

void bind_arguments_to_env(disp_env_t *env, disp_val params, disp_val *args, int arg_count) {
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
            ERRO("bind_arguments_to_env: rest parameter is not a symbol, got type %d", T(p));
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
            ERRO("bind_arguments_to_env: parameter is not a symbol");
            return;
        }
        uint64_t id = SYM_ID(sym);
        disp_val val = (idx < arg_count) ? args[idx] : NIL;
        disp_define_symbol(env, id, val, 0);
        idx++;
        p = disp_cdr(p);
    }

    // 绑定 rest 参数（如果有）
    if (NE(rest_sym, NIL)) {
        uint64_t rest_id = SYM_ID(rest_sym);
        disp_val rest_list = NIL;
        for (int j = arg_count - 1; j >= fixed; j--) {
            rest_list = disp_make_cons(args[j], rest_list);
        }
        disp_define_symbol(env, rest_id, rest_list, 0);
    }
}

#include "tail/tail.h"

/* 从参数数组调用内置函数（构造临时表达式） */
static disp_val disp_apply_builtin_from_array(disp_val builtin, disp_env_t *env, disp_val *args, int arg_count) {
    // 1. 构建参数列表 (arg1 arg2 ... argN)
    disp_val arg_list = NIL;
    for (int i = arg_count - 1; i >= 0; i--) {
        arg_list = disp_make_cons(args[i], arg_list);
    }
    // 2. 构建完整调用表达式 (builtin arg1 arg2 ...)
    disp_val expr = disp_make_cons(builtin, arg_list);
    // 3. 调用内置函数
    disp_val result = disp_get_builtin(builtin)(env, expr);
    // 临时构造的 cons 节点会在下次 GC 时自动回收（无根引用）
    return result;
}

disp_val disp_apply_closure(disp_val closure, disp_val *args, int arg_count) {
    if (!disp_is_closure_reuse_env(closure)) {
        GC_ROOT(disp_env_t, new_env) = disp_new_env(disp_get_closure_env(closure));
        bind_arguments_to_env(new_env, disp_get_closure_params(closure), args, arg_count);
        disp_val ret = disp_eval_body(new_env, disp_get_closure_body(closure));
        return ret;
    }

    disp_env_t *env = disp_get_closure_env(closure);
    disp_val params = disp_get_closure_params(closure);
    disp_val body = disp_get_closure_body(closure);
    disp_val *current_args = args;
    int current_argc = arg_count;
    eval_result_t res;

    // 用于保护当前参数数组的根指针（静态或局部静态，但需要线程安全）
    static _Thread_local disp_val *protected_args = NULL;  // 注意：多线程下需要 TLS 或锁

    // 保护初始参数数组（如果非空）
    if (current_args != NULL) {
        //gc_add_root(&protected_args);
        //protected_args = current_args;
    }

    while (1) {
        bind_arguments_to_env(env, params, current_args, current_argc);

        disp_val exprs = body;
        while (NN(exprs) && T(exprs) == FLAG_CONS) {
            disp_val expr = disp_car(exprs);
            disp_val next = disp_cdr(exprs);
            int tail = E(next, NIL);
            res = disp_eval_tail(env, expr, closure, tail);
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
                    //protected_args = current_args;
                    //gc_add_root(&protected_args);
                }

                // 更新循环所需的新闭包内部数据
                if (T(closure) == FLAG_CLOSURE) {
                    env = disp_get_closure_env(closure);
                    params = disp_get_closure_params(closure);
                    body = disp_get_closure_body(closure);
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
