
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

eval_result_t disp_eval_tail_letreca(disp_env_t *env, disp_val expr, int is_tail, disp_val current_closure) {
    disp_val op = disp_car(expr);
    disp_val args = disp_cdr(expr);

    // 特殊形式处理
    if (T(op) == FLAG_SYMBOL) {

        // letrec* : 顺序绑定，每个初值在已包含前面变量的作用域中求值
        if (E(op, LETRECA)) {
            if (N(args) || T(args) != FLAG_CONS) {
                ERRO("malformed letrec*");
                return RESULT_NORMAL(NIL);
            }

            disp_val first = disp_car(args);
    
            // 命名 let: (let name ((var val) ...) body ...)
            if (T(first) == FLAG_SYMBOL) {
                disp_val rest = disp_cdr(args);
                if (NN(rest) && T(rest) == FLAG_CONS) {
                    // 调用 disp_letf 处理命名 let，返回最终结果
                    disp_val result = disp_letf(env, expr);
                    return RESULT_NORMAL(result);
                }
            }

            disp_val bindings = first;
            disp_val body_exprs = disp_cdr(args);
            if (N(body_exprs)) {
                ERRO("letrec*: missing body");
                return RESULT_NORMAL(NIL);
            }

            // 创建新作用域
            GC_ROOT(disp_env_t, new_env) = disp_new_env(env);

            // 顺序处理每个绑定
            disp_val b = bindings;
            while (NN(b) && T(b) == FLAG_CONS) {
                disp_val pair = disp_car(b);
                if (T(pair) != FLAG_CONS || T(disp_car(pair)) != FLAG_SYMBOL) {
                    ERRO("malformed letrec* binding");
                    return RESULT_NORMAL(NIL);
                }
                disp_val sym = disp_car(pair);
                uint64_t id = SI(sym);
                disp_val init_expr = disp_car(disp_cdr(pair));
                // 先绑定占位符 NIL（使变量在作用域内可用）
                disp_define_symbol(new_env, id, NIL, 0);
                // 求值初值（此时变量已存在，值为 NIL，但表达式可引用自身或其他已绑定的变量）
                disp_val val = disp_eval(new_env, init_expr);
                // 更新绑定为实际值
                disp_define_symbol(new_env, id, val, 1);
                b = disp_cdr(b);
            }

            // 对 body 序列进行尾求值
            while (NN(body_exprs) && T(body_exprs) == FLAG_CONS) {
                disp_val cur = disp_car(body_exprs);
                disp_val next = disp_cdr(body_exprs);
                if (E(next, NIL)) {
                    return disp_eval_tail(new_env, cur, is_tail, current_closure);
                } else {
                    disp_eval(new_env, cur);
                    body_exprs = next;
                }
            }
            ERRO("not letreca branch");
            return RESULT_NORMAL(NIL);
        }
        ERRO("not letreca branch");
        return RESULT_NORMAL(NIL);
    }
    ERRO("not letreca branch");
    return RESULT_NORMAL(NIL);
}
