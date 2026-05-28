
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

eval_result_t disp_eval_tail_let(disp_scope_t *env, disp_val expr, int is_tail, disp_val current_closure) {
    disp_val op = disp_car(expr);
    disp_val args = disp_cdr(expr);

    // 特殊形式处理
    if (T(op) == FLAG_SYMBOL) {
        // let (简单形式，支持并行绑定)
        if (E(op, LET)) {
            if (N(args) || T(args) != FLAG_CONS) {
                ERRO("malformed let");
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
                ERRO("let: missing body");
                return RESULT_NORMAL(NIL);
            }

            // 计算绑定数量
            int var_count = 0;
            for (disp_val b = bindings; NN(b) && T(b) == FLAG_CONS; b = disp_cdr(b)) var_count++;

            if (var_count == 0) {
                // 没有绑定，直接对 body 进行尾求值
                while (NN(body_exprs) && T(body_exprs) == FLAG_CONS) {
                    disp_val cur = disp_car(body_exprs);
                    disp_val next = disp_cdr(body_exprs);
                    if (E(next, NIL)) {
                        return disp_eval_tail(env, cur, is_tail, current_closure);
                    } else {
                        disp_eval(env, cur);
                        body_exprs = next;
                    }
                }
                return RESULT_NORMAL(NIL);
            }

            // 分配临时数组并加入 GC 根
            GC_ROOT(disp_val, var_syms) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
            GC_ROOT(disp_val, init_vals) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);

            // 解析绑定，并在旧环境中求值初值（并行绑定）
            int idx = 0;
            disp_val b = bindings;
            while (NN(b) && T(b) == FLAG_CONS) {
                disp_val pair = disp_car(b);
                if (T(pair) != FLAG_CONS || T(disp_car(pair)) != FLAG_SYMBOL) {
                    gc_free(init_vals);
                    gc_free(var_syms);
                    ERRO("malformed let binding");
                    return RESULT_NORMAL(NIL);
                }
                var_syms[idx] = disp_car(pair);
                // 在当前环境 env 中求值初值（并行）
                init_vals[idx] = disp_eval(env, disp_car(disp_cdr(pair)));
                idx++;
                b = disp_cdr(b);
            }

            // 创建新作用域，父作用域为当前 env
            GC_ROOT(disp_scope_t, new_scope) = disp_new_scope(env);

            // 将所有变量绑定到新作用域（并行，一次性）
            for (int i = 0; i < var_count; i++) {
                const char *name = disp_get_symbol_name(var_syms[i]);
                disp_define_symbol(new_scope, name, init_vals[i], 0);
            }

            gc_free(init_vals);
            gc_free(var_syms);

            // 对 body 序列进行尾求值（类似于 begin）
            while (NN(body_exprs) && T(body_exprs) == FLAG_CONS) {
                disp_val cur = disp_car(body_exprs);
                disp_val next = disp_cdr(body_exprs);
                if (E(next, NIL)) {
                    // 最后一个表达式，保持 is_tail 和 current_closure
                    return disp_eval_tail(new_scope, cur, is_tail, current_closure);
                } else {
                    disp_eval(new_scope, cur);
                    body_exprs = next;
                }
            }
            return RESULT_NORMAL(NIL);
        }
        ERRO("not let branch");
        return RESULT_NORMAL(NIL);
    }
    ERRO("not let branch");
    return RESULT_NORMAL(NIL);
}
