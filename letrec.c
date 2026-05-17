
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

eval_result_t* disp_eval_tail_letrec(disp_scope_t *env, disp_val *expr, int is_tail, disp_val *current_closure) {
    disp_val *op = disp_car(expr);
    disp_val *args = disp_cdr(expr);

    // 特殊形式处理
    if (T(op) == DISP_SYMBOL) {
        const char *opname = disp_get_symbol_name(op);
        // letrec : 并行绑定，所有初值在同一作用域内求值，变量可互相引用
        // letrec : 并行绑定（使用堆分配 + GC 根保护）
        if (strcmp(opname, "letrec") == 0) {
            if (!args || T(args) != DISP_CONS) {
                ERRO("malformed letrec");
                return result_nil();
            }
            disp_val *bindings = disp_car(args);
            disp_val *body_exprs = disp_cdr(args);
            if (!body_exprs) {
                ERRO("letrec: missing body");
                return result_nil();
            }

            // 统计绑定数量
            int var_count = 0;
            for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b)) var_count++;
            if (var_count == 0) {
                // 无绑定，直接求值 body
                while (body_exprs && T(body_exprs) == DISP_CONS) {
                    disp_val *cur = disp_car(body_exprs);
                    disp_val *next = disp_cdr(body_exprs);
                    if (next == NIL)
                        return disp_eval_tail(env, cur, is_tail, current_closure);
                    else
                        disp_eval(env, cur);
                    body_exprs = next;
                }
                return result_nil();
            }

            // 分配临时数组并加入 GC 根
            disp_val **var_syms = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
            disp_val **init_exprs = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
            gc_add_root(&var_syms);
            gc_add_root(&init_exprs);

            // 解析绑定
            int idx = 0;
            disp_val *b = bindings;
            while (b && T(b) == DISP_CONS) {
                disp_val *pair = disp_car(b);
                if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
                    gc_remove_root(&init_exprs);
                    gc_remove_root(&var_syms);
                    gc_free(init_exprs);
                    gc_free(var_syms);
                    ERRO("malformed letrec binding");
                    return result_nil();
                }
                var_syms[idx] = disp_car(pair);
                init_exprs[idx] = disp_car(disp_cdr(pair));
                idx++;
                b = disp_cdr(b);
            }

            // 创建新作用域
            disp_scope_t *new_scope = disp_new_scope(env);
            gc_add_root(&new_scope);
            // 先绑定所有变量为 NIL（占位符）
            for (int i = 0; i < var_count; i++) {
                const char *name = disp_get_symbol_name(var_syms[i]);
                disp_define_symbol(new_scope, name, NIL, 0);
            }

            // 并行求值所有初值
            disp_val **init_vals = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
            gc_add_root(&init_vals);
            for (int i = 0; i < var_count; i++) {
                init_vals[i] = disp_eval(new_scope, init_exprs[i]);
                gc_add_root(&init_vals[i]);   // 每个值也要保护（可选，但安全）
            }

            // 更新绑定为实际值
            for (int i = 0; i < var_count; i++) {
                const char *name = disp_get_symbol_name(var_syms[i]);
                disp_define_symbol(new_scope, name, init_vals[i], 1);
            }

            // 清理临时数组（先移除根再释放）
            for (int i = 0; i < var_count; i++) {
                gc_remove_root(&init_vals[i]);
            }
            gc_remove_root(&init_vals);
            gc_free(init_vals);
            gc_remove_root(&init_exprs);
            gc_free(init_exprs);
            gc_remove_root(&var_syms);
            gc_free(var_syms);

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
            return result_nil();
        }
        ERRO("not letrec branch");
        return result_nil();
    }
    ERRO("not letrec branch");
    return result_nil();
}
