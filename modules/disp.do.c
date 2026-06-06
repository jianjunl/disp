
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

extern void bind_arguments_to_env(disp_env_t *env, disp_val params, disp_val *args, int arg_count);

// ======================== do 循环 ========================
// 语法: (do ((var init step) ...) (test result ...) body ...)
static disp_val do_builtin(disp_env_t *env, disp_val expr) {
    disp_val rest = disp_cdr(expr);
    if (N(rest) || T(rest) != FLAG_CONS)
        ERET(NIL, "do: missing variable list");
    disp_val var_specs = disp_car(rest);   // ((var init step) ...)
    rest = disp_cdr(rest);
    if (N(rest) || T(rest) != FLAG_CONS)
        ERET(NIL, "do: missing termination clause");
    disp_val volatile term_clause = disp_car(rest); // (test result ...)
    disp_val body = disp_cdr(rest);        // body forms

    // 保护整个尾部（包含 var_specs, term_clause, body）
    gc_add_root(&rest);

    int volatile var_count = 0;
    for (disp_val s = var_specs; NN(s) && T(s) == FLAG_CONS; s = disp_cdr(s))
        var_count++;

    // 无变量情况：直接在当前作用域中循环
    if (var_count == 0) {
        volatile disp_val result = NIL;
        volatile int normal_exit = 0;
        TRY {
            while (1) {
                disp_val test = disp_eval(env, disp_car(term_clause));
                if (NE(test, NIL)) {
                    disp_val results = disp_cdr(term_clause);
                    result = NIL;
                    while (NN(results) && T(results) == FLAG_CONS) {
                        result = disp_eval(env, disp_car(results));
                        results = disp_cdr(results);
                    }
                    break;
                }
                disp_val b = body;
                while (NN(b) && T(b) == FLAG_CONS) {
                    disp_eval(env, disp_car(b));
                    b = disp_cdr(b);
                }
            }
            normal_exit = 1;
        } CATCH {
            THROW_THROWN();
        }
        END_TRY;
        if (normal_exit) {
            gc_remove_root(&rest);
            return result;
        }
        gc_remove_root(&rest);
        return NIL;
    }

    // 收集变量信息：符号、初值表达式、步进表达式
    disp_sid *var_names = gc_malloc(var_count * sizeof(disp_sid));
    disp_val *init_exprs = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    disp_val *step_exprs = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    gc_add_root(&var_names);
    gc_add_root(&init_exprs);
    gc_add_root(&step_exprs);

    int i = 0;
    for (disp_val s = var_specs; NN(s) && T(s) == FLAG_CONS; s = disp_cdr(s), i++) {
        disp_val spec = disp_car(s);
        if (T(spec) != FLAG_CONS) {
            gc_remove_root(&step_exprs);
            gc_remove_root(&init_exprs);
            gc_remove_root(&var_names);
            gc_remove_root(&rest);
            gc_free(var_names); gc_free(init_exprs); gc_free(step_exprs);
            ERET(NIL, "do: malformed variable spec");
        }
        disp_val var = disp_car(spec);
        if (T(var) != FLAG_SYMBOL) {
            gc_remove_root(&step_exprs);
            gc_remove_root(&init_exprs);
            gc_remove_root(&var_names);
            gc_remove_root(&rest);
            gc_free(var_names); gc_free(init_exprs); gc_free(step_exprs);
            ERET(NIL, "do: variable name must be a symbol");
        }
        var_names[i] = SYM_ID(var);
        disp_val init_part = disp_cdr(spec);
        if (N(init_part) || T(init_part) != FLAG_CONS) {
            gc_remove_root(&step_exprs);
            gc_remove_root(&init_exprs);
            gc_remove_root(&var_names);
            gc_remove_root(&rest);
            gc_free(var_names); gc_free(init_exprs); gc_free(step_exprs);
            ERET(NIL, "do: missing init expression");
        }
        init_exprs[i] = disp_car(init_part);
        disp_val step_part = disp_cdr(init_part);
        if (NN(step_part) && T(step_part) == FLAG_CONS)
            step_exprs[i] = disp_car(step_part);
        else
            step_exprs[i] = DNULL;
    }

    // 创建新作用域（子作用域，父作用域为当前 env）
    disp_env_t *loop_env = disp_new_env(env);
    gc_add_root(&loop_env);

    // 计算所有初值（在当前 env 中求值）
    disp_val *init_vals = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    gc_add_root(&init_vals);
    for (int j = 0; j < var_count; j++) {
        init_vals[j] = disp_eval(env, init_exprs[j]);
    }

    // 将初值一次性绑定到 loop_env（构造参数列表）
    disp_val params = NIL;
    for (int j = var_count - 1; j >= 0; j--)
        params = disp_make_cons(disp_intern_symbol(loop_env, var_names[j]), params);
    bind_arguments_to_env(loop_env, params, init_vals, var_count);

    // 主循环（在 loop_env 中进行）
    disp_val  volatile result = NIL;
    volatile int normal_exit = 0;
    TRY {
        while (1) {
            // 在当前循环作用域中求值终止测试
            disp_val test = disp_eval(loop_env, disp_car(term_clause));
            if (NE(test, NIL)) {
                // 求值结果表达式（仍在 loop_env 中）
                disp_val results = disp_cdr(term_clause);
                result = NIL;
                while (NN(results) && T(results) == FLAG_CONS) {
                    result = disp_eval(loop_env, disp_car(results));
                    results = disp_cdr(results);
                }
                break;
            }
            // 执行 body
            disp_val b = body;
            while (NN(b) && T(b) == FLAG_CONS) {
                disp_eval(loop_env, disp_car(b));
                b = disp_cdr(b);
            }
            // 更新变量（并行更新：先求值所有新值，再赋值）
            disp_val *new_vals = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
            gc_add_root(&new_vals);
            for (int j = 0; j < var_count; j++) {
                if (NN(step_exprs[j])) {
                    new_vals[j] = disp_eval(loop_env, step_exprs[j]);
                } else {
                    // 无步进，保持当前值（从作用域中取出）
                    disp_val sym = disp_find_symbol(loop_env, var_names[j]);
                    new_vals[j] = NN(sym) ? SYM_VALUE(sym) : NIL;
                }
                gc_add_root(&new_vals[j]);
            }
            for (int j = 0; j < var_count; j++) {
                disp_define_symbol(loop_env, var_names[j], new_vals[j], 0);
                gc_remove_root(&new_vals[j]);
            }
            gc_remove_root(&new_vals);
            gc_free(new_vals);
        }
        normal_exit = 1;
    } CATCH {
        // 异常路径：释放动态资源，传播异常
        gc_remove_root(&loop_env);
        gc_remove_root(&init_vals);
        gc_remove_root(&step_exprs);
        gc_remove_root(&init_exprs);
        gc_remove_root(&var_names);
        gc_remove_root(&rest);
        gc_free(init_vals);
        gc_free(step_exprs);
        gc_free(init_exprs);
        gc_free(var_names);
        THROW_THROWN();
    }
    END_TRY;

    if (normal_exit) {
        // 释放资源
        for (int j = 0; j < var_count; j++) {
            gc_remove_root(&init_vals[j]);
        }
        gc_remove_root(&init_vals);
        gc_remove_root(&loop_env);
        gc_remove_root(&step_exprs);
        gc_remove_root(&init_exprs);
        gc_remove_root(&var_names);
        gc_remove_root(&rest);
        gc_free(init_vals);
        gc_free(step_exprs);
        gc_free(init_exprs);
        gc_free(var_names);
        return result;
    }
    return NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF2(DO      , MKB(do_builtin     , "<#do>"     ), 1);
}
