
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

eval_result_t* disp_eval_tail_flow(disp_scope_t *env, disp_box expr, int is_tail, disp_box current_closure) {
    disp_box op = disp_car(expr);
    disp_box args = disp_cdr(expr);

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
            disp_box test = disp_car(args);
            disp_box then_branch = disp_car(disp_cdr(args));
            disp_box else_branch = (disp_cdr(disp_cdr(args)) != NIL) ? disp_car(disp_cdr(disp_cdr(args))) : NIL;
            disp_box test_val = disp_eval(env, test);
            disp_box branch = test_val != NIL ? then_branch : else_branch;
            // 尾位置继续传递
            return disp_eval_tail(env, branch, is_tail, current_closure);
        }

        // begin
        if (op == BEGIN || op == PROGN) {
            if (!args) return result_nil();
            disp_box exprs = args;
            while (exprs && T(exprs) == DISP_CONS) {
                disp_box cur = disp_car(exprs);
                disp_box next = disp_cdr(exprs);
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
            disp_box clauses = args;
            while (clauses && T(clauses) == DISP_CONS) {
                disp_box clause = disp_car(clauses);
                if (T(clause) != DISP_CONS) ERRO("malformed cond clause");
                disp_box test = disp_car(clause);
                disp_box rest_exprs = disp_cdr(clause);
                if (test == ELSE || disp_eval(env, test) != NIL) {
                    if (rest_exprs == NIL) {
                        // 返回 test 值（R5RS 风格）
                        return result_normal(disp_eval(env, test));
                    } else {
                        // 处理隐式 begin
                        disp_box body = rest_exprs;
                        while (body && T(body) == DISP_CONS) {
                            disp_box cur = disp_car(body);
                            disp_box next = disp_cdr(body);
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
            disp_box exprs = args;
            disp_box last_val = TRUE;
            while (exprs && T(exprs) == DISP_CONS) {
                disp_box cur = disp_car(exprs);
                disp_box next = disp_cdr(exprs);
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
            disp_box exprs = args;
            disp_box last_val = NIL;
            while (exprs && T(exprs) == DISP_CONS) {
                disp_box cur = disp_car(exprs);
                disp_box next = disp_cdr(exprs);
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
            disp_box val_expr = disp_car(disp_cdr(args));
            disp_box val = disp_eval(env, val_expr);
            disp_box sym = disp_find_symbol(env, varname);
            if (!sym) ERRO("set! on unbound variable: %s", varname);
            disp_set_symbol_value(sym, val);
            return result_normal(val);
        }

        // define (仅允许顶层，但这里按局部处理)
        if (op == DEFINE) {
            if (!args || T(args) != DISP_CONS) ERRO("malformed define");
            disp_box first = disp_car(args);
            if (T(first) == DISP_SYMBOL) {
                const char *name = disp_get_symbol_name(first);
                disp_box val_expr = disp_car(disp_cdr(args));
                disp_box val = disp_eval(env, val_expr);
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
            disp_box params = disp_car(args);
            disp_box body = disp_cdr(args);
            disp_box closure = disp_make_closure(env, params, body, 0);
            return result_normal(closure);
        }

        ERRO("not a flow");
        return result_nil();
    }
    ERRO("not a flow");
    return result_nil();
}

