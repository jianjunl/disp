
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

eval_result_t* disp_eval_tail_letreca(disp_scope_t *env, disp_val *expr, int is_tail, disp_val *current_closure) {
    disp_val *op = disp_car(expr);
    disp_val *args = disp_cdr(expr);

    // 特殊形式处理
    if (T(op) == DISP_SYMBOL) {

        // letrec* : 顺序绑定，每个初值在已包含前面变量的作用域中求值
        if (op == LETRECA) {
            if (!args || T(args) != DISP_CONS) {
                ERRO("malformed letrec*");
                return result_nil();
            }
            disp_val *bindings = disp_car(args);
            disp_val *body_exprs = disp_cdr(args);
            if (!body_exprs) {
                ERRO("letrec*: missing body");
                return result_nil();
            }

            // 创建新作用域
            disp_scope_t *new_scope = disp_new_scope(env);
            GC_ROOT_AUTO(new_scope);

            // 顺序处理每个绑定
            disp_val *b = bindings;
            while (b && T(b) == DISP_CONS) {
                disp_val *pair = disp_car(b);
                if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
                    ERRO("malformed letrec* binding");
                    return result_nil();
                }
                disp_val *sym = disp_car(pair);
                const char *name = disp_get_symbol_name(sym);
                disp_val *init_expr = disp_car(disp_cdr(pair));
                // 先绑定占位符 NIL（使变量在作用域内可用）
                disp_define_symbol(new_scope, name, NIL, 0);
                // 求值初值（此时变量已存在，值为 NIL，但表达式可引用自身或其他已绑定的变量）
                disp_val *val = disp_eval(new_scope, init_expr);
                // 更新绑定为实际值
                disp_define_symbol(new_scope, name, val, 1);
                b = disp_cdr(b);
            }

            // 对 body 序列进行尾求值
            while (body_exprs && T(body_exprs) == DISP_CONS) {
                disp_val *cur = disp_car(body_exprs);
                disp_val *next = disp_cdr(body_exprs);
                if (next == NIL) {
                    return disp_eval_tail(new_scope, cur, is_tail, current_closure);
                } else {
                    disp_eval(new_scope, cur);
                    body_exprs = next;
                }
            }
            ERRO("not letreca branch");
            return result_nil();
        }
        ERRO("not letreca branch");
        return result_nil();
    }
    ERRO("not letreca branch");
    return result_nil();
}
