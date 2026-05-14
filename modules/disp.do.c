
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

// ======================== do 循环 ========================
// 语法: (do ((var init step) ...) (test result ...) body ...)
static disp_val* do_builtin(disp_scope_t *scope, disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "do: missing variable list");
    disp_val *var_specs = disp_car(rest);   // ((var init step) ...)
    rest = disp_cdr(rest);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "do: missing termination clause");
    disp_val *term_clause = disp_car(rest); // (test result ...)
    disp_val *body = disp_cdr(rest);        // body forms

    // --- 保护整个表达式尾部（包含 var_specs, term_clause, body） ---
    gc_add_root(&rest);

    int var_count = 0;
    for (disp_val *s = var_specs; s && T(s) == DISP_CONS; s = disp_cdr(s))
        var_count++;

    if (var_count == 0) {
        // 无变量，仍需要异常安全（body 内部可能 throw）
        gc_remove_root(&rest);
        disp_val *result = NIL;
        volatile int normal_exit = 0;
        TRY {
            while (1) {
                disp_val *test = disp_eval(NULL, disp_car(term_clause));
                if (test != NIL) {
                    disp_val *results = disp_cdr(term_clause);
                    result = NIL;
                    while (results && T(results) == DISP_CONS) {
                        result = disp_eval(NULL, disp_car(results));
                        results = disp_cdr(results);
                    }
                    break;
                }
                disp_val *b = body;
                while (b && T(b) == DISP_CONS) {
                    disp_eval(NULL, disp_car(b));
                    b = disp_cdr(b);
                }
            }
            normal_exit = 1;
        } CATCH {
            THROW_THROWN();
        }
        END_TRY;
        if (normal_exit) return result;
        return NIL;
    }

    // 收集变量信息
    char **var_names = gc_typed_malloc(var_count * sizeof(char*), &GC_TYPE_PTR_ARRAY);
    disp_val **init_vals = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    disp_val **step_exprs = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    gc_add_root(&var_names);
    gc_add_root(&init_vals);
    gc_add_root(&step_exprs);

    int i = 0;
    for (disp_val *s = var_specs; s && T(s) == DISP_CONS; s = disp_cdr(s), i++) {
        disp_val *spec = disp_car(s);
        if (T(spec) != DISP_CONS) {
            gc_remove_root(&step_exprs);
            gc_remove_root(&init_vals);
            gc_remove_root(&var_names);
            gc_remove_root(&rest);
            gc_free(var_names); gc_free(init_vals); gc_free(step_exprs);
            ERET(NIL, "do: malformed variable spec");
        }
        disp_val *var = disp_car(spec);
        if (T(var) != DISP_SYMBOL) {
            gc_remove_root(&step_exprs);
            gc_remove_root(&init_vals);
            gc_remove_root(&var_names);
            gc_remove_root(&rest);
            gc_free(var_names); gc_free(init_vals); gc_free(step_exprs);
            ERET(NIL, "do: variable name must be a symbol");
        }
        var_names[i] = gc_strdup(disp_get_symbol_name(var));
        disp_val *init_part = disp_cdr(spec);
        if (!init_part || T(init_part) != DISP_CONS) {
            gc_remove_root(&step_exprs);
            gc_remove_root(&init_vals);
            gc_remove_root(&var_names);
            gc_remove_root(&rest);
            gc_free(var_names); gc_free(init_vals); gc_free(step_exprs);
            ERET(NIL, "do: missing init expression");
        }
        init_vals[i] = disp_car(init_part);
        disp_val *step_part = disp_cdr(init_part);
        if (step_part && T(step_part) == DISP_CONS)
            step_exprs[i] = disp_car(step_part);
        else
            step_exprs[i] = NULL;
    }

    // 保存旧值并求值初始值
    disp_val **old_vals = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    gc_add_root(&old_vals);
    for (int j = 0; j < var_count; j++) {
        disp_val *old_sym = disp_find_symbol(NULL, var_names[j]);
        old_vals[j] = old_sym ? disp_get_symbol_value(old_sym) : NULL;
        if (old_vals[j] != NULL) gc_add_root(&old_vals[j]);
        disp_val *init_val = disp_eval(NULL, init_vals[j]);
        disp_define_symbol(NULL, var_names[j], init_val, 0);
    }

    // 主循环（异常安全）
    disp_val * volatile result = NIL;
    volatile int normal_exit = 0;
    TRY {
        while (1) {
            disp_val *test = disp_eval(NULL, disp_car(term_clause));
            if (test != NIL) {
                // 求值结果表达式
                disp_val *results = disp_cdr(term_clause);
                result = NIL;
                while (results && T(results) == DISP_CONS) {
                    result = disp_eval(NULL, disp_car(results));
                    results = disp_cdr(results);
                }
                break;
            }
            // 执行 body
            disp_val *b = body;
            while (b && T(b) == DISP_CONS) {
                disp_eval(NULL, disp_car(b));
                b = disp_cdr(b);
            }
            // 更新变量（并行更新：先求值所有新值，再赋值）
            disp_val **new_vals = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
            gc_add_root(&new_vals);
            for (int j = 0; j < var_count; j++) {
                if (step_exprs[j]) {
                    new_vals[j] = disp_eval(NULL, step_exprs[j]);
                } else {
                    // 无步进，保持当前值
                    disp_val *sym = disp_find_symbol(NULL, var_names[j]);
                    new_vals[j] = sym ? disp_get_symbol_value(sym) : NIL;
                }
                gc_add_root(&new_vals[j]);
            }
            for (int j = 0; j < var_count; j++) {
                disp_define_symbol(NULL, var_names[j], new_vals[j], 0);
                gc_remove_root(&new_vals[j]);
            }
            gc_remove_root(&new_vals);
            gc_free(new_vals);
        }
        // 正常退出：恢复旧值，清理资源
        for (int j = 0; j < var_count; j++) {
            disp_val *restore = old_vals[j];
            disp_define_symbol(NULL, var_names[j], restore ? restore : NIL, 0);
            if (restore != NULL) gc_remove_root(&old_vals[j]);
            gc_free(var_names[j]);
        }
        gc_remove_root(&old_vals);
        gc_remove_root(&step_exprs);
        gc_remove_root(&init_vals);
        gc_remove_root(&var_names);
        gc_remove_root(&rest);
        gc_free(var_names); gc_free(init_vals); gc_free(step_exprs); gc_free(old_vals);
        normal_exit = 1;
    } CATCH {
        // 异常路径：恢复绑定，清理资源，传播异常
        for (int j = 0; j < var_count; j++) {
            disp_val *restore = old_vals[j];
            disp_define_symbol(NULL, var_names[j], restore ? restore : NIL, 0);
            if (restore != NULL) gc_remove_root(&old_vals[j]);
            gc_free(var_names[j]);
        }
        gc_remove_root(&old_vals);
        gc_remove_root(&step_exprs);
        gc_remove_root(&init_vals);
        gc_remove_root(&var_names);
        gc_remove_root(&rest);
        gc_free(var_names); gc_free(init_vals); gc_free(step_exprs); gc_free(old_vals);
        THROW_THROWN();
    }
    END_TRY;
    if (normal_exit) return result;
    return NIL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclobbered"
#pragma GCC diagnostic pop
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("do"      , MKB(do_builtin     , "<#do>"     ), 1);
}
