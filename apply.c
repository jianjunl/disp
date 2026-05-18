
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
    disp_val **args = gc_typed_malloc(*arg_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    gc_add_root(&args);   // 临时保护
    int i = 0;
    for (disp_val *a = arg_list; a && T(a) == DISP_CONS; a = disp_cdr(a)) {
        args[i++] = disp_eval(env, disp_car(a));
    }
    gc_remove_root(&args); // 移除保护，返回值将在调用者中成为根
    return args;
}

extern eval_result_t* disp_eval_tail_let(disp_scope_t *env, disp_val *expr, int is_tail, disp_val *current_closure);
extern eval_result_t* disp_eval_tail_leta(disp_scope_t *env, disp_val *expr, int is_tail, disp_val *current_closure);
extern eval_result_t* disp_eval_tail_letrec(disp_scope_t *env, disp_val *expr, int is_tail, disp_val *current_closure);
extern eval_result_t* disp_eval_tail_letreca(disp_scope_t *env, disp_val *expr, int is_tail, disp_val *current_closure);

eval_result_t* result_nil() {
    static eval_result_t ret  = (eval_result_t){.kind = 0, .normal = NULL};
    if(!ret.normal) ret.normal = NIL;
    return &ret;
}

static eval_result_t* result_true() {
    static eval_result_t ret  = (eval_result_t){.kind = 0, .normal = NULL};
    if(!ret.normal) ret.normal = TRUE;
    return &ret;
}

GC_TYPE_INFO(eval_result_ti, eval_result_t,
    offsetof(eval_result_t, normal),       // disp_val*
    offsetof(eval_result_t, tail.new_args) // disp_val**
);

static eval_result_t* result_normal(disp_val *normal) {
    eval_result_t *res = gc_typed_malloc(sizeof(eval_result_t), &eval_result_ti);
    res->kind = 0;
    res->normal = normal;
    return res;
}

static eval_result_t* result_tail(disp_val **new_argv, int new_argc) {
    eval_result_t *res = gc_typed_malloc(sizeof(eval_result_t), &eval_result_ti);
    res->kind = 1;
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

        // quote
        if (op == QUOTE) {
            if (!args || T(args) != DISP_CONS) ERRO("malformed quote");
            return result_normal(disp_car(args));
        }

        // if
        if (op == IF) {
            if (!args || T(args) != DISP_CONS) ERRO("malformed if");
            disp_val *test = disp_car(args);
            disp_val *then_branch = disp_car(disp_cdr(args));
            disp_val *else_branch = (disp_cdr(disp_cdr(args)) != NIL) ? disp_car(disp_cdr(disp_cdr(args))) : NIL;
            disp_val *test_val = disp_eval(env, test);
            disp_val *branch = test_val != NIL ? then_branch : else_branch;
            // 尾位置继续传递
            return disp_eval_tail(env, branch, is_tail, current_closure);
        }

        // begin
        if (op == BEGIN || op == PROGN) {
            if (!args) return result_nil();
            disp_val *exprs = args;
            while (exprs && T(exprs) == DISP_CONS) {
                disp_val *cur = disp_car(exprs);
                disp_val *next = disp_cdr(exprs);
                if (next == NIL) {
                    // 最后一个表达式，保持 is_tail
                    return disp_eval_tail(env, cur, is_tail, current_closure);
                } else {
                    // 非尾表达式，忽略结果
                    disp_eval(env, cur);
                    exprs = next;
                }
            }
            return result_nil();
        }

        // cond – 简单展开为嵌套 if
        if (op == COND) {
            if (!args) return result_nil();
            disp_val *clauses = args;
            while (clauses && T(clauses) == DISP_CONS) {
                disp_val *clause = disp_car(clauses);
                if (T(clause) != DISP_CONS) ERRO("malformed cond clause");
                disp_val *test = disp_car(clause);
                disp_val *rest_exprs = disp_cdr(clause);
                if (test == ELSE || disp_eval(env, test) != NIL) {
                    if (rest_exprs == NIL) {
                        // 返回 test 值（R5RS 风格）
                        return result_normal(disp_eval(env, test));
                    } else {
                        // 处理隐式 begin
                        disp_val *body = rest_exprs;
                        while (body && T(body) == DISP_CONS) {
                            disp_val *cur = disp_car(body);
                            disp_val *next = disp_cdr(body);
                            if (next == NIL)
                                return disp_eval_tail(env, cur, is_tail, current_closure);
                            else
                                disp_eval(env, cur);
                            body = next;
                        }
                    }
                }
                clauses = disp_cdr(clauses);
            }
            return result_nil();
        }

        // and
        if (op == AND) {
            if (args == NIL) return result_true();
            disp_val *exprs = args;
            disp_val *last_val = TRUE;
            while (exprs && T(exprs) == DISP_CONS) {
                disp_val *cur = disp_car(exprs);
                disp_val *next = disp_cdr(exprs);
                last_val = disp_eval(env, cur);
                if (last_val == NIL) {
                    return result_normal(last_val);
                }
                if (next == NIL) {
                    return disp_eval_tail(env, cur, is_tail, current_closure);
                }
                exprs = next;
            }
            return result_normal(last_val);
        }
        // or
        if (op == OR) {
            if (args == NIL) return result_nil();
            disp_val *exprs = args;
            disp_val *last_val = NIL;
            while (exprs && T(exprs) == DISP_CONS) {
                disp_val *cur = disp_car(exprs);
                disp_val *next = disp_cdr(exprs);
                last_val = disp_eval(env, cur);
                if (last_val != NIL) {
                    return disp_eval_tail(env, cur, is_tail, current_closure);
                }
                if (next == NIL) {
                    return disp_eval_tail(env, cur, is_tail, current_closure);
                }
                exprs = next;
            }
            return result_normal(last_val);
        }

        // set!
        if (op == SET || op == SETQ) {
            if (!args || T(args) != DISP_CONS || T(disp_car(args)) != DISP_SYMBOL) ERRO("malformed set!");
            const char *varname = disp_get_symbol_name(disp_car(args));
            disp_val *val_expr = disp_car(disp_cdr(args));
            disp_val *val = disp_eval(env, val_expr);
            disp_val *sym = disp_find_symbol(env, varname);
            if (!sym) ERRO("set! on unbound variable: %s", varname);
            disp_set_symbol_value(sym, val);
            return result_normal(val);
        }

        // define (仅允许顶层，但这里按局部处理)
        if (op == DEFINE) {
            if (!args || T(args) != DISP_CONS) ERRO("malformed define");
            disp_val *first = disp_car(args);
            if (T(first) == DISP_SYMBOL) {
                const char *name = disp_get_symbol_name(first);
                disp_val *val_expr = disp_car(disp_cdr(args));
                disp_val *val = disp_eval(env, val_expr);
                disp_define_symbol(env, name, val, 0);
                return result_normal(val);
            } else if (T(first) == DISP_CONS) {
                // 函数定义 (define (f args) body)
                // 简化：暂不实现
                ERRO("define with function syntax not supported in tail evaluator");
            } else {
                ERRO("malformed define");
            }
        }

        // lambda
        if (op == LAMBDA) {
            if (!args || T(args) != DISP_CONS) ERRO("malformed lambda");
            disp_val *params = disp_car(args);
            disp_val *body = disp_cdr(args);
            disp_val *closure = disp_make_closure(env, params, body, 0);
            return result_normal(closure);
        }

        // let (简单形式，支持并行绑定)
        if (op == LET) {
            return disp_eval_tail_let(env, expr, is_tail, current_closure);
        }
        // let* : 顺序绑定，每个初值在扩展后的作用域中求值
        if (op == LETA) {
            return disp_eval_tail_leta(env, expr, is_tail, current_closure);
        }
        // letrec : 并行绑定，所有初值在同一作用域内求值，变量可互相引用
        if (op == LETREC) {
            return disp_eval_tail_letrec(env, expr, is_tail, current_closure);
        }
        // letrec* : 顺序绑定，每个初值在已包含前面变量的作用域中求值
        if (op == LETRECA) {
            return disp_eval_tail_letreca(env, expr, is_tail, current_closure);
        }

    }

    // 函数调用
    // 先求值操作符
    disp_val *func = disp_eval(env, op);

    if (is_tail && func == current_closure) {
        // 尾递归自调用：提取参数并返回重启标记
        int new_argc = 0;
        disp_val **new_argv = eval_args_for_tail(env, args, &new_argc);
        return result_tail(new_argv, new_argc);
    } else {
        // 正常调用：求值参数并应用
        disp_val *result;
        if (T(func) == DISP_BUILTIN) {
            result = disp_get_builtin(func)(env, expr);
            return result_normal(result);
        } else {
            int arg_count = 0;
            for (disp_val *a = args; a && T(a) == DISP_CONS; a = disp_cdr(a)) arg_count++;
            disp_val **argv = gc_typed_malloc(arg_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
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
                char *s = disp_string(func);
                ERRO("%s is not a function or macro", s);
                return result_nil();
            }
            gc_free(argv);
            return result_normal(result);
        }
    }
}
