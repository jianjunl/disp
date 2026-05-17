
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

// 在 closure.c 或单独的头文件中定义
typedef struct eval_result {
    int kind;   // 0 = normal, 1 = tail_recurse
    union {
        disp_val *normal;
        struct {
            disp_val **new_args;
            int arg_count;
        } tail;
    };
} eval_result_t;

extern eval_result_t disp_eval_tail(disp_scope_t *env, disp_val *expr, int is_tail, disp_val *current_closure);

eval_result_t disp_eval_tail_let(disp_scope_t *env, disp_val *expr, int is_tail, disp_val *current_closure) {
    disp_val *op = disp_car(expr);
    disp_val *args = disp_cdr(expr);

    // 特殊形式处理
    if (T(op) == DISP_SYMBOL) {
        const char *opname = disp_get_symbol_name(op);
        // let (简单形式，支持并行绑定)
        if (strcmp(opname, "let") == 0) {
            if (!args || T(args) != DISP_CONS) {
                ERRO("malformed let");
                return (eval_result_t){.kind = 0, .normal = NIL};
            }
            disp_val *bindings = disp_car(args);
            disp_val *body_exprs = disp_cdr(args);
            if (!body_exprs) {
                ERRO("let: missing body");
                return (eval_result_t){.kind = 0, .normal = NIL};
            }

            // 计算绑定数量
            int var_count = 0;
            for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b)) var_count++;

            if (var_count == 0) {
                // 没有绑定，直接对 body 进行尾求值
                while (body_exprs && T(body_exprs) == DISP_CONS) {
                    disp_val *cur = disp_car(body_exprs);
                    disp_val *next = disp_cdr(body_exprs);
                    if (next == NIL) {
                        return disp_eval_tail(env, cur, is_tail, current_closure);
                    } else {
                        disp_eval(env, cur);
                        body_exprs = next;
                    }
                }
                return (eval_result_t){.kind = 0, .normal = NIL};
            }

            // 分配临时数组并加入 GC 根
            disp_val **var_syms = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
            disp_val **init_vals = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
            gc_add_root(&var_syms);
            gc_add_root(&init_vals);

            // 解析绑定，并在旧环境中求值初值（并行绑定）
            int idx = 0;
            disp_val *b = bindings;
            while (b && T(b) == DISP_CONS) {
                disp_val *pair = disp_car(b);
                if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
                    gc_remove_root(&init_vals);
                    gc_remove_root(&var_syms);
                    gc_free(init_vals);
                    gc_free(var_syms);
                    ERRO("malformed let binding");
                    return (eval_result_t){.kind = 0, .normal = NIL};
                }
                var_syms[idx] = disp_car(pair);
                // 在当前环境 env 中求值初值（并行）
                init_vals[idx] = disp_eval(env, disp_car(disp_cdr(pair)));
                // 保护每个初值（可选但安全）
                gc_add_root(&init_vals[idx]);
                idx++;
                b = disp_cdr(b);
            }

            // 创建新作用域，父作用域为当前 env
            disp_scope_t *new_scope = disp_new_scope(env);
            // 将所有变量绑定到新作用域（并行，一次性）
            for (int i = 0; i < var_count; i++) {
                const char *name = disp_get_symbol_name(var_syms[i]);
                disp_define_symbol(new_scope, name, init_vals[i], 0);
            }

            // 清理临时数组（先移除每个初值的保护，再释放数组）
            for (int i = 0; i < var_count; i++) {
                gc_remove_root(&init_vals[i]);
            }
            gc_remove_root(&init_vals);
            gc_free(init_vals);
            gc_remove_root(&var_syms);
            gc_free(var_syms);

            // 对 body 序列进行尾求值（类似于 begin）
            while (body_exprs && T(body_exprs) == DISP_CONS) {
                disp_val *cur = disp_car(body_exprs);
                disp_val *next = disp_cdr(body_exprs);
                if (next == NIL) {
                    // 最后一个表达式，保持 is_tail 和 current_closure
                    return disp_eval_tail(new_scope, cur, is_tail, current_closure);
                } else {
                    disp_eval(new_scope, cur);
                    body_exprs = next;
                }
            }
            return (eval_result_t){.kind = 0, .normal = NIL};
        }
        ERRO("not let branch");
        return (eval_result_t){.kind = 0, .normal = NIL};
    }
    ERRO("not let branch");
    return (eval_result_t){.kind = 0, .normal = NIL};
}
