
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

eval_result_t disp_eval_tail_letrec(disp_env_t *env, disp_val expr, int is_tail, disp_val current_closure) {
    disp_val op = disp_car(expr);
    disp_val args = disp_cdr(expr);

    // 特殊形式处理
    if (T(op) == FLAG_SYMBOL) {
        // letrec : 并行绑定，所有初值在同一作用域内求值，变量可互相引用
        // letrec : 并行绑定（使用堆分配 + GC 根保护）
        if (E(op, LETREC)) {
            if (N(args) || T(args) != FLAG_CONS) {
                ERRO("malformed letrec");
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
                ERRO("letrec: missing body");
                return RESULT_NORMAL(NIL);
            }

            // 统计绑定数量
            int var_count = 0;
            for (disp_val b = bindings; NN(b) && T(b) == FLAG_CONS; b = disp_cdr(b)) var_count++;
            if (var_count == 0) {
                // 无绑定，直接求值 body
                while (NN(body_exprs) && T(body_exprs) == FLAG_CONS) {
                    disp_val cur = disp_car(body_exprs);
                    disp_val next = disp_cdr(body_exprs);
                    if (E(next, NIL))
                        return disp_eval_tail(env, cur, is_tail, current_closure);
                    else
                        disp_eval(env, cur);
                    body_exprs = next;
                }
                return RESULT_NORMAL(NIL);
            }

            // 分配临时数组并加入 GC 根
            GC_ROOT(disp_val, var_syms) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
            GC_ROOT(disp_val, init_exprs) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);

            // 解析绑定
            int idx = 0;
            disp_val b = bindings;
            while (NN(b) && T(b) == FLAG_CONS) {
                disp_val pair = disp_car(b);
                if (T(pair) != FLAG_CONS || T(disp_car(pair)) != FLAG_SYMBOL) {
                    gc_free(init_exprs);
                    gc_free(var_syms);
                    ERRO("malformed letrec binding");
                    return RESULT_NORMAL(NIL);
                }
                var_syms[idx] = disp_car(pair);
                init_exprs[idx] = disp_car(disp_cdr(pair));
                idx++;
                b = disp_cdr(b);
            }

            // 创建新作用域
            GC_ROOT(disp_env_t, new_env) = disp_new_env(env);

            // 先绑定所有变量为 NIL（占位符）
            for (int i = 0; i < var_count; i++) {
                uint64_t id = SI(var_syms[i]);
                disp_define_symbol(new_env, id, NIL, 0);
            }

            // 并行求值所有初值
            GC_ROOT(disp_val, init_vals) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
            for (int i = 0; i < var_count; i++) {
                init_vals[i] = disp_eval(new_env, init_exprs[i]);
            }

            // 更新绑定为实际值
            for (int i = 0; i < var_count; i++) {
                uint64_t id = SI(var_syms[i]);
                disp_define_symbol(new_env, id, init_vals[i], 1);
            }

            // 清理临时数组（先移除根再释放）
            gc_free(init_vals);
            gc_free(init_exprs);
            gc_free(var_syms);

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
            return RESULT_NORMAL(NIL);
        }
        ERRO("not letrec branch");
        return RESULT_NORMAL(NIL);
    }
    ERRO("not letrec branch");
    return RESULT_NORMAL(NIL);
}
