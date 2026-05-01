
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
static disp_val* do_builtin(disp_val *expr) {
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
                disp_val *test = disp_eval(disp_car(term_clause));
                if (test != NIL) {
                    disp_val *results = disp_cdr(term_clause);
                    result = NIL;
                    while (results && T(results) == DISP_CONS) {
                        result = disp_eval(disp_car(results));
                        results = disp_cdr(results);
                    }
                    break;
                }
                disp_val *b = body;
                while (b && T(b) == DISP_CONS) {
                    disp_eval(disp_car(b));
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
    char **var_names = gc_malloc(var_count * sizeof(char*));
    disp_val **init_vals = gc_malloc(var_count * sizeof(disp_val*));
    disp_val **step_exprs = gc_malloc(var_count * sizeof(disp_val*));
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
    disp_val **old_vals = gc_malloc(var_count * sizeof(disp_val*));
    gc_add_root(&old_vals);
    for (int j = 0; j < var_count; j++) {
        disp_val *old_sym = disp_find_symbol(var_names[j]);
        old_vals[j] = old_sym ? disp_get_symbol_value(old_sym) : NULL;
        if (old_vals[j] != NULL) gc_add_root(&old_vals[j]);
        disp_val *init_val = disp_eval(init_vals[j]);
        disp_define_symbol(var_names[j], init_val, 0);
    }

    // 主循环（异常安全）
    disp_val * volatile result = NIL;
    volatile int normal_exit = 0;
    TRY {
        while (1) {
            disp_val *test = disp_eval(disp_car(term_clause));
            if (test != NIL) {
                // 求值结果表达式
                disp_val *results = disp_cdr(term_clause);
                result = NIL;
                while (results && T(results) == DISP_CONS) {
                    result = disp_eval(disp_car(results));
                    results = disp_cdr(results);
                }
                break;
            }
            // 执行 body
            disp_val *b = body;
            while (b && T(b) == DISP_CONS) {
                disp_eval(disp_car(b));
                b = disp_cdr(b);
            }
            // 更新变量（并行更新：先求值所有新值，再赋值）
            disp_val **new_vals = gc_malloc(var_count * sizeof(disp_val*));
            gc_add_root(&new_vals);
            for (int j = 0; j < var_count; j++) {
                if (step_exprs[j]) {
                    new_vals[j] = disp_eval(step_exprs[j]);
                } else {
                    // 无步进，保持当前值
                    disp_val *sym = disp_find_symbol(var_names[j]);
                    new_vals[j] = sym ? disp_get_symbol_value(sym) : NIL;
                }
                gc_add_root(&new_vals[j]);
            }
            for (int j = 0; j < var_count; j++) {
                disp_define_symbol(var_names[j], new_vals[j], 0);
                gc_remove_root(&new_vals[j]);
            }
            gc_remove_root(&new_vals);
            gc_free(new_vals);
        }
        // 正常退出：恢复旧值，清理资源
        for (int j = 0; j < var_count; j++) {
            disp_val *restore = old_vals[j];
            disp_define_symbol(var_names[j], restore ? restore : NIL, 0);
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
            disp_define_symbol(var_names[j], restore ? restore : NIL, 0);
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

// ======================== dotimes 循环 ========================
static disp_val* dotimes_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "dotimes: missing binding list");
    disp_val *binding = disp_car(rest);
    if (T(binding) != DISP_CONS)
        ERET(NIL, "dotimes: malformed binding");
    disp_val *var = disp_car(binding);
    if (T(var) != DISP_SYMBOL)
        ERET(NIL, "dotimes: variable must be a symbol");
    const char *var_name = disp_get_symbol_name(var);
    disp_val *count_expr = disp_car(disp_cdr(binding));
    if (!count_expr)
        ERET(NIL, "dotimes: missing count");
    disp_val *result_expr = NIL;
    disp_val *rest_binding = disp_cdr(disp_cdr(binding));
    if (rest_binding && T(rest_binding) == DISP_CONS)
        result_expr = disp_car(rest_binding);
    disp_val *body = disp_cdr(rest);  // 剩余 body

    // 保护 body 表达式以及用到的临时对象
    gc_add_root(&rest);                // 保护整个尾部
    // 求值 count
    disp_val *count_val = disp_eval(count_expr);
    long limit;
    disp_flag_t ct = T(count_val);
    if (ct == DISP_INT) {
        limit = disp_get_int(count_val);
    } else if (ct == DISP_LONG) {
        limit = disp_get_long(count_val);
    } else {
        gc_remove_root(&rest);
        ERET(NIL, "dotimes: count must be an integer");
    }

    // 保存旧值并保护
    disp_val *old_sym = disp_find_symbol(var_name);
    disp_val *old_val = old_sym ? disp_get_symbol_value(old_sym) : NULL;
    if (old_val != NULL) gc_add_root(&old_val);

    disp_val * volatile last_result = NIL;
    volatile int normal_exit = 0;
    TRY {
        if (limit <= 0) {
            if (result_expr)
                last_result = disp_eval(result_expr);
            else
                last_result = NIL;
        } else {
            for (long i = 0; i < limit; i++) {
                disp_define_symbol(var_name, disp_make_long(i), 0);
                disp_val *b = body;
                while (b && T(b) == DISP_CONS) {
                    last_result = disp_eval(disp_car(b));
                    b = disp_cdr(b);
                }
            }
            // 循环正常结束，取 result 表达式
            if (result_expr)
                last_result = disp_eval(result_expr);
        }
        // 恢复旧值，清理保护
        if (old_sym) {
            disp_define_symbol(var_name, old_val ? old_val : NIL, 0);
        } else {
            disp_define_symbol(var_name, NIL, 0);
        }
        if (old_val != NULL) gc_remove_root(&old_val);
        gc_remove_root(&rest);
        normal_exit = 1;
    } CATCH {
        // 异常：恢复绑定
        if (old_sym) {
            disp_define_symbol(var_name, old_val ? old_val : NIL, 0);
        } else {
            disp_define_symbol(var_name, NIL, 0);
        }
        if (old_val != NULL) gc_remove_root(&old_val);
        gc_remove_root(&rest);
        THROW_THROWN();
    }
    END_TRY;
    if (normal_exit) return last_result;
    return NIL;
}

// ======================== dolist 循环 ========================
static disp_val* dolist_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "dolist: missing binding list");
    disp_val *binding = disp_car(rest);
    if (T(binding) != DISP_CONS)
        ERET(NIL, "dolist: malformed binding");
    disp_val *var = disp_car(binding);
    if (T(var) != DISP_SYMBOL)
        ERET(NIL, "dolist: variable must be a symbol");
    const char *var_name = disp_get_symbol_name(var);
    disp_val *list_expr = disp_car(disp_cdr(binding));
    if (!list_expr)
        ERET(NIL, "dolist: missing list");
    disp_val *result_expr = NIL;
    disp_val *rest_binding = disp_cdr(disp_cdr(binding));
    if (rest_binding && T(rest_binding) == DISP_CONS)
        result_expr = disp_car(rest_binding);
    disp_val *body = disp_cdr(rest);

    // 保护
    gc_add_root(&rest);
    disp_val *lst = disp_eval(list_expr);

    disp_val *old_sym = disp_find_symbol(var_name);
    disp_val *old_val = old_sym ? disp_get_symbol_value(old_sym) : NULL;
    if (old_val != NULL) gc_add_root(&old_val);

    disp_val * volatile last_result = NIL;
    volatile int normal_exit = 0;
    TRY {
        for (disp_val *p = lst; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
            disp_val *elem = disp_car(p);
            disp_define_symbol(var_name, elem, 0);
            disp_val *b = body;
            while (b && T(b) == DISP_CONS) {
                last_result = disp_eval(disp_car(b));
                b = disp_cdr(b);
            }
        }
        if (result_expr)
            last_result = disp_eval(result_expr);
        // 恢复
        if (old_sym) {
            disp_define_symbol(var_name, old_val ? old_val : NIL, 0);
        } else {
            disp_define_symbol(var_name, NIL, 0);
        }
        if (old_val != NULL) gc_remove_root(&old_val);
        gc_remove_root(&rest);
        normal_exit = 1;
    } CATCH {
        if (old_sym) {
            disp_define_symbol(var_name, old_val ? old_val : NIL, 0);
        } else {
            disp_define_symbol(var_name, NIL, 0);
        }
        if (old_val != NULL) gc_remove_root(&old_val);
        gc_remove_root(&rest);
        THROW_THROWN();
    }
    END_TRY;
    if (normal_exit) return last_result;
    return NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("do"      , MKB(do_builtin     , "<#do>"     ), 1);
    DEF("dotimes" , MKB(dotimes_builtin, "<#dotimes>"), 1);
    DEF("dolist"  , MKB(dolist_builtin , "<#dolist>" ), 1);
}
