
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

    // 第一遍：收集变量名、初始值、步进表达式
    int var_count = 0;
    for (disp_val *s = var_specs; s && T(s) == DISP_CONS; s = disp_cdr(s))
        var_count++;
    if (var_count == 0) {
        // 没有变量，直接循环
        while (1) {
            // 求值终止条件
            disp_val *test = disp_eval(disp_car(term_clause));
            if (test != NIL) {
                // 返回结果表达式
                disp_val *results = disp_cdr(term_clause);
                if (!results) return NIL;
                disp_val *last = NIL;
                while (results && T(results) == DISP_CONS) {
                    last = disp_eval(disp_car(results));
                    results = disp_cdr(results);
                }
                return last;
            }
            // 执行 body
            disp_val *body_it = body;
            while (body_it && T(body_it) == DISP_CONS) {
                disp_eval(disp_car(body_it));
                body_it = disp_cdr(body_it);
            }
        }
    }

    // 分配存储
    char **var_names = gc_malloc(var_count * sizeof(char*));
    disp_val **init_vals = gc_malloc(var_count * sizeof(disp_val*));
    disp_val **step_exprs = gc_malloc(var_count * sizeof(disp_val*));
    int i = 0;
    for (disp_val *s = var_specs; s && T(s) == DISP_CONS; s = disp_cdr(s), i++) {
        disp_val *spec = disp_car(s);
        if (T(spec) != DISP_CONS) {
            gc_free(var_names); gc_free(init_vals); gc_free(step_exprs);
            ERET(NIL, "do: malformed variable spec");
        }
        disp_val *var = disp_car(spec);
        if (T(var) != DISP_SYMBOL) {
            gc_free(var_names); gc_free(init_vals); gc_free(step_exprs);
            ERET(NIL, "do: variable name must be a symbol");
        }
        var_names[i] = gc_strdup(disp_get_symbol_name(var));
        // 初始值表达式
        disp_val *init_part = disp_cdr(spec);
        if (!init_part || T(init_part) != DISP_CONS) {
            gc_free(var_names); gc_free(init_vals); gc_free(step_exprs);
            ERET(NIL, "do: missing init expression");
        }
        init_vals[i] = disp_car(init_part);
        // 步进表达式（可选）
        disp_val *step_part = disp_cdr(init_part);
        if (step_part && T(step_part) == DISP_CONS)
            step_exprs[i] = disp_car(step_part);
        else
            step_exprs[i] = NULL;  // 无步进
    }

    // 保存旧值并绑定初始值
    disp_val **old_vals = gc_malloc(var_count * sizeof(disp_val*));
    for (int j = 0; j < var_count; j++) {
        disp_val *old_sym = disp_find_symbol(var_names[j]);
        old_vals[j] = old_sym ? disp_get_symbol_value(old_sym) : NULL;
        disp_define_symbol(var_names[j], disp_eval(init_vals[j]), 0);
    }

    // 主循环
    disp_val *result = NIL;
    while (1) {
        // 检查终止条件
        disp_val *test = disp_eval(disp_car(term_clause));
        if (test != NIL) {
            // 求值结果表达式
            disp_val *results = disp_cdr(term_clause);
            if (!results) result = NIL;
            else {
                result = NIL;
                while (results && T(results) == DISP_CONS) {
                    result = disp_eval(disp_car(results));
                    results = disp_cdr(results);
                }
            }
            break;
        }
        // 执行 body
        disp_val *body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
        // 更新变量（并行更新需要保存新值，这里简化：顺序更新，但每个新值基于旧值）
        // 标准 do 是并行，但顺序更新对于简单用法足够，且更简单。
        disp_val **new_vals = gc_malloc(var_count * sizeof(disp_val*));
        for (int j = 0; j < var_count; j++) {
            if (step_exprs[j]) {
                new_vals[j] = disp_eval(step_exprs[j]);
            } else {
                // 无步进，保持当前值
                disp_val *sym = disp_find_symbol(var_names[j]);
                new_vals[j] = sym ? disp_get_symbol_value(sym) : NIL;
            }
        }
        for (int j = 0; j < var_count; j++) {
            disp_define_symbol(var_names[j], new_vals[j], 0);
        }
        gc_free(new_vals);
    }

    // 恢复旧值
    for (int j = 0; j < var_count; j++) {
        if (old_vals[j] != NULL)
            disp_define_symbol(var_names[j], old_vals[j], 0);
        else
            disp_define_symbol(var_names[j], NIL, 0);
        gc_free(var_names[j]);
    }
    gc_free(var_names);
    gc_free(init_vals);
    gc_free(step_exprs);
    gc_free(old_vals);
    return result;
}

// ======================== dotimes 循环 ========================
// 语法: (dotimes (var count [result]) body ...)
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
    disp_val *body = disp_cdr(rest);  // 剩余 body 形式

    // 求值 count
    disp_val *count_val = disp_eval(count_expr);
    long limit;
    disp_flag_t ct = T(count_val);
    if (ct == DISP_INT) {
        limit = disp_get_int(count_val);
    } else if (ct == DISP_LONG) {
        limit = disp_get_long(count_val);
    } else {
        ERET(NIL, "dotimes: count must be an integer");
    }
    if (limit <= 0) {
        // 直接返回 result 或 nil
        if (result_expr)
            return disp_eval(result_expr);
        else
            return NIL;
    }

    // 保存旧值
    disp_val *old_sym = disp_find_symbol(var_name);
    disp_val *old_val = old_sym ? disp_get_symbol_value(old_sym) : NULL;
    // 循环
    disp_val *last_result = NIL;
    for (long i = 0; i < limit; i++) {
        disp_define_symbol(var_name, disp_make_long(i), 0);
        // 执行 body
        disp_val *body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            last_result = disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    // 恢复旧值
    if (old_sym)
        disp_define_symbol(var_name, old_val, 0);
    else
        disp_define_symbol(var_name, NIL, 0);
    // 返回结果
    if (result_expr)
        return disp_eval(result_expr);
    else
        return last_result ? last_result : NIL;
}

// ======================== dolist 循环 ========================
// 语法: (dolist (var list [result]) body ...)
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
    disp_val *body = disp_cdr(rest);  // 剩余 body

    // 求值列表
    disp_val *lst = disp_eval(list_expr);
    // 保存旧值
    disp_val *old_sym = disp_find_symbol(var_name);
    disp_val *old_val = old_sym ? disp_get_symbol_value(old_sym) : NULL;
    // 遍历
    disp_val *last_result = NIL;
    for (disp_val *p = lst; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        disp_val *elem = disp_car(p);
        disp_define_symbol(var_name, elem, 0);
        // 执行 body
        disp_val *body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            last_result = disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    // 恢复旧值
    if (old_sym)
        disp_define_symbol(var_name, old_val, 0);
    else
        disp_define_symbol(var_name, NIL, 0);
    // 返回结果
    if (result_expr)
        return disp_eval(result_expr);
    else
        return last_result ? last_result : NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("do"      , MKB(do_builtin     , "<#do>"     ), 1);
    DEF("dotimes" , MKB(dotimes_builtin, "<#dotimes>"), 1);
    DEF("dolist"  , MKB(dolist_builtin , "<#dolist>" ), 1);
}
