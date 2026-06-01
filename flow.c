
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

eval_result_t disp_eval_tail_flow(disp_env_t *env, disp_val expr, int is_tail, disp_val current_closure) {
    disp_val op = disp_car(expr);
    disp_val args = disp_cdr(expr);

    // 特殊形式处理
    if (T(op) == FLAG_SYMBOL) {

        // quote
        if (E(op, QUOTE)) {
            if (N(args) || T(args) != FLAG_CONS) ERRO("malformed quote");
            return RESULT_NORMAL(disp_car(args));
        }

        // if
        if (E(op, IF)) {
            if (N(args) || T(args) != FLAG_CONS) ERRO("malformed if");
            disp_val test = disp_car(args);
            disp_val then_branch = disp_car(disp_cdr(args));
            disp_val else_branch = (NE(disp_cdr(disp_cdr(args)), NIL)) ? disp_car(disp_cdr(disp_cdr(args))) : NIL;
            disp_val test_val = disp_eval(env, test);
            disp_val branch = NE(test_val, NIL) ? then_branch : else_branch;
            // 尾位置继续传递
            return disp_eval_tail(env, branch, is_tail, current_closure);
        }

        // begin
        if (E(op, BEGIN) || E(op, PROGN)) {
            if (N(args)) return RESULT_NORMAL(NIL);
            disp_val exprs = args;
            while (NN(exprs) && T(exprs) == FLAG_CONS) {
                disp_val cur = disp_car(exprs);
                disp_val next = disp_cdr(exprs);
                if (E(next, NIL)) {
                    // 最后一个表达式，保持 is_tail
                    return disp_eval_tail(env, cur, is_tail, current_closure);
                } else {
                    // 非尾表达式，忽略结果
                    disp_eval(env, cur);
                    exprs = next;
                }
            }
            return RESULT_NORMAL(NIL);
        }

        // cond – 简单展开为嵌套 if
        if (E(op, COND)) {
            if (N(args)) return RESULT_NORMAL(NIL);
            disp_val clauses = args;
            while (NN(clauses) && T(clauses) == FLAG_CONS) {
                disp_val clause = disp_car(clauses);
                if (T(clause) != FLAG_CONS) ERRO("malformed cond clause");
                disp_val test = disp_car(clause);
                disp_val rest_exprs = disp_cdr(clause);
                if (E(test, ELSE) || NE(disp_eval(env, test), NIL)) {
                    if (E(rest_exprs, NIL)) {
                        // 返回 test 值（R5RS 风格）
                        return RESULT_NORMAL(disp_eval(env, test));
                    } else {
                        // 处理隐式 begin
                        disp_val body = rest_exprs;
                        while (NN(body) && T(body) == FLAG_CONS) {
                            disp_val cur = disp_car(body);
                            disp_val next = disp_cdr(body);
                            if (E(next, NIL))
                                return disp_eval_tail(env, cur, is_tail, current_closure);
                            else
                                disp_eval(env, cur);
                            body = next;
                        }
                    }
                }
                clauses = disp_cdr(clauses);
            }
            return RESULT_NORMAL(NIL);
        }

        // and
        if (E(op, AND)) {
            if (E(args, NIL)) return RESULT_NORMAL(TRUE);
            disp_val exprs = args;
            disp_val last_val = TRUE;
            while (NN(exprs) && T(exprs) == FLAG_CONS) {
                disp_val cur = disp_car(exprs);
                disp_val next = disp_cdr(exprs);
                last_val = disp_eval(env, cur);
                if (E(last_val, NIL)) {
                    return RESULT_NORMAL(last_val);
                }
                if (E(next, NIL)) {
                    return disp_eval_tail(env, cur, is_tail, current_closure);
                }
                exprs = next;
            }
            return RESULT_NORMAL(last_val);
        }
        // or
        if (E(op, OR)) {
            if (E(args, NIL)) return RESULT_NORMAL(NIL);
            disp_val exprs = args;
            disp_val last_val = NIL;
            while (NN(exprs) && T(exprs) == FLAG_CONS) {
                disp_val cur = disp_car(exprs);
                disp_val next = disp_cdr(exprs);
                last_val = disp_eval(env, cur);
                if (NE(last_val, NIL)) {
                    return disp_eval_tail(env, cur, is_tail, current_closure);
                }
                if (E(next, NIL)) {
                    return disp_eval_tail(env, cur, is_tail, current_closure);
                }
                exprs = next;
            }
            return RESULT_NORMAL(last_val);
        }

        // set!
        if (E(op, SET) || E(op, SETQ)) {
            if (N(args) || T(args) != FLAG_CONS || T(disp_car(args)) != FLAG_SYMBOL) ERRO("malformed set!");
            uint64_t varid = SI(disp_car(args));
            disp_val val_expr = disp_car(disp_cdr(args));
            disp_val val = disp_eval(env, val_expr);
            disp_val sym = disp_find_symbol(env, varid);
            if (N(sym)) ERRO("set! on unbound variable: %s", disp_get_name(varid));
            disp_set_symbol_value(sym, val);
            return RESULT_NORMAL(val);
        }

        // define (仅允许顶层，但这里按局部处理)
        if (E(op, DEFINE)) {
            if (N(args) || T(args) != FLAG_CONS) ERRO("malformed define");
            disp_val first = disp_car(args);
            if (T(first) == FLAG_SYMBOL) {
                uint64_t id = SI(first);
                disp_val val_expr = disp_car(disp_cdr(args));
                disp_val val = disp_eval(env, val_expr);
                disp_define_symbol(env, id, val, 0);
                return RESULT_NORMAL(val);
            } else if (T(first) == FLAG_CONS) {
                // 函数定义 (define (f args) body)
                // 简化：暂不实现
                ERRO("define with function syntax not supported in tail evaluator");
            } else {
                ERRO("malformed define");
            }
        }

        // lambda
        if (E(op, LAMBDA)) {
            if (N(args) || T(args) != FLAG_CONS) ERRO("malformed lambda");
            disp_val params = disp_car(args);
            disp_val body = disp_cdr(args);
            disp_val closure = disp_make_closure(env, params, body, 0);
            return RESULT_NORMAL(closure);
        }

        ERRO("not a flow");
        return RESULT_NORMAL(NIL);
    }
    ERRO("not a flow");
    return RESULT_NORMAL(NIL);
}

