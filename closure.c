
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
        disp_box params;
        disp_box body;
        disp_scope_t *env;
        int reuse_scope;    /* 1: 可复用调用时的作用域（优化尾递归） */
    } closure;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, closure.params),
    GC_OFF(disp_data, closure.body),
    GC_OFF(disp_data, closure.env)
);

static void intern_params(disp_scope_t *env, disp_box params) {
    for (disp_box p = params; p && T(p) == FLAG_CONS; p = disp_cdr(p)) {
        disp_box sym = disp_car(p);
        if (T(sym) == FLAG_SYMBOL) {
            const char *name = disp_get_symbol_name(sym);
            if (!disp_find_symbol(env, name)) {
                disp_define_symbol(env, name, NIL, 0);
            }
        }
    }
}

disp_box disp_make_closure(disp_scope_t *env, disp_box params, disp_box body, int reuse_scope) {
    intern_params(env, params);
    disp_box v = ALLOC_TI(FLAG_CLOSURE);
    v->data->closure.params = params;
    v->data->closure.body   = body;
    v->data->closure.env    = env;
    v->data->closure.reuse_scope = reuse_scope;
    return v;
}

disp_box disp_make_macro(disp_scope_t *env, disp_box params, disp_box body, int reuse_scope) {
    intern_params(env, params);
    disp_box v = ALLOC_TI(FLAG_MACRO);
    v->data->closure.params = params;
    v->data->closure.body   = body;
    v->data->closure.env    = env;
    v->data->closure.reuse_scope = reuse_scope;
    return v;
}

disp_box disp_get_closure_params(disp_box closure) {
    if (T(closure) != FLAG_CLOSURE && T(closure) != FLAG_MACRO) {
        ERRO("disp_get_closure_params: not a closure/macro\n");
        return NIL;
    }
    return closure->data->closure.params;
}

disp_box disp_get_closure_body(disp_box closure) {
    if (T(closure) != FLAG_CLOSURE && T(closure) != FLAG_MACRO) {
        ERRO("disp_get_closure_body: not a closure/macro\n");
        return NIL;
    }
    return closure->data->closure.body;
}

disp_scope_t* disp_get_closure_env(disp_box closure) {
    if (T(closure) != FLAG_CLOSURE && T(closure) != FLAG_MACRO) {
        ERRO("disp_get_closure_env: not a closure/macro\n");
        return NULL;
    }
    return closure->data->closure.env;
}

void bind_arguments_to_scope(disp_scope_t *scope, disp_box params, disp_box *args, int arg_count) {
    int fixed = 0;
    disp_box rest_sym = NIL;

    // 遍历参数列表，计算固定参数个数，并找到可能的 rest 参数
    disp_box p = params;
    while (p && T(p) == FLAG_CONS) {
        fixed++;
        p = disp_cdr(p);
    }
    if (p != NIL) {
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
    while (p && T(p) == FLAG_CONS && idx < fixed) {
        disp_box sym = disp_car(p);
        if (T(sym) != FLAG_SYMBOL) {
            ERRO("bind_arguments_to_scope: parameter is not a symbol");
            return;
        }
        const char *name = disp_get_symbol_name(sym);
        disp_box val = (idx < arg_count) ? args[idx] : NIL;
        disp_define_symbol(scope, name, val, 0);
        idx++;
        p = disp_cdr(p);
    }

    // 绑定 rest 参数（如果有）
    if (rest_sym != NIL) {
        const char *rest_name = disp_get_symbol_name(rest_sym);
        disp_box rest_list = NIL;
        for (int j = arg_count - 1; j >= fixed; j--) {
            rest_list = disp_make_cons(args[j], rest_list);
        }
        disp_define_symbol(scope, rest_name, rest_list, 0);
    }
}

#include "tail.h"

disp_box disp_apply_closure(disp_box closure, disp_box *args, int arg_count) {
    if (!closure->data->closure.reuse_scope) {
        GC_ROOT(disp_scope_t, new_scope) = disp_new_scope(closure->data->closure.env);
        bind_arguments_to_scope(new_scope, closure->data->closure.params, args, arg_count);
        disp_box ret = disp_eval_body(new_scope, closure->data->closure.body);
        return ret;
    }

    disp_scope_t *env = closure->data->closure.env;
    disp_box params = closure->data->closure.params;
    disp_box body = closure->data->closure.body;
    disp_box *current_args = args;
    int current_argc = arg_count;
    eval_result_t *res = NULL;   // 用于释放

    // 用于保护当前参数数组的根指针（静态或局部静态，但需要线程安全）
    static _Thread_local disp_box *protected_args = NULL;  // 注意：多线程下需要 TLS 或锁
    static _Thread_local eval_result_t *protected_res = NULL;

    // 保护初始参数数组（如果非空）
    if (current_args != NULL) {
        gc_add_root(&protected_args);
        protected_args = current_args;
    }

    while (1) {
        bind_arguments_to_scope(env, params, current_args, current_argc);

        disp_box exprs = body;
        while (exprs && T(exprs) == FLAG_CONS) {
            disp_box expr = disp_car(exprs);
            disp_box next = disp_cdr(exprs);
            int tail = (next == NIL);
            res = disp_eval_tail(env, expr, tail, closure);
            // 保护新的 res
            if (protected_res) gc_remove_root(&protected_res);
            protected_res = res;
            if (protected_res) gc_add_root(&protected_res);
            if (res->kind == 1) {
                // 释放旧的参数数组前，先移除其根
                if (current_args != args && current_args != NULL) {
                    if (protected_args) gc_remove_root(&protected_args);
                    protected_args = NULL;
                    gc_free(current_args);
                }

                // 取出新目标和新参数
                closure = res->tail.target;
                current_args = res->tail.new_args;
                current_argc = res->tail.arg_count;

                // 保护新数组
                if (current_args != NULL) {
                    if (protected_args) gc_remove_root(&protected_args);
                    protected_args = current_args;
                    gc_add_root(&protected_args);
                }

                // 更新循环所需的新闭包内部数据
                if (T(closure) == FLAG_CLOSURE) {
                    env = closure->data->closure.env;
                    params = closure->data->closure.params;
                    body = closure->data->closure.body;
                    gc_free(res);
                    goto restart;
                } 
                else if (T(closure) == FLAG_BUILTIN) {
                    // 内置函数：调用后直接返回（不继续蹦床）
                    disp_box result = disp_apply_builtin_from_array(closure, env, current_args, current_argc);
                    gc_free(res);
                    // 清理当前参数数组（如果是从堆分配的）
                    if (current_args != args && current_args != NULL)
                        gc_free(current_args);
                    return result;
                }
                else if (T(closure) == FLAG_SYSCALL) {
                    // 系统调用：参数数组直接可用
                    disp_box result = disp_get_syscall(closure)(current_args, current_argc);
                    gc_free(res);
                    if (current_args != args && current_args != NULL)
                        gc_free(current_args);
                    return result;
                }
                else {
                    // 非法目标
                    gc_free(res);
                    ERET(NIL, "tail call target is not callable");
                }
            } else if (tail) {
                disp_box result = res->normal;
                if (protected_res) {
                    gc_remove_root(&protected_res);
                    protected_res = NULL;
                }
                gc_free(res);
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
                if (protected_res) {
                    gc_remove_root(&protected_res);
                    protected_res = NULL;
                }
                gc_free(res);          // 释放非尾结果的 result 对象
                res = NULL;
                exprs = next;
            }
        }
        // body 为空时（理论上不应发生），返回 NIL
        if (res) {
            if (protected_res) {
                gc_remove_root(&protected_res);
                protected_res = NULL;
            }
            gc_free(res);
        }
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
    if (protected_res) gc_remove_root(&protected_res);
    return NIL;
}
