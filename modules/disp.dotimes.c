
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

// ======================== dotimes 循环 ========================
static disp_val* dotimes_builtin(disp_scope_t *scope, disp_val *expr) {
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
    disp_val *count_val = disp_eval(NULL, count_expr);
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
    disp_val *old_sym = disp_find_symbol(NULL, var_name);
    disp_val *old_val = old_sym ? disp_get_symbol_value(old_sym) : NULL;
    if (old_val != NULL) gc_add_root(&old_val);

    disp_val * volatile last_result = NIL;
    volatile int normal_exit = 0;
    TRY {
        if (limit <= 0) {
            if (result_expr)
                last_result = disp_eval(NULL, result_expr);
            else
                last_result = NIL;
        } else {
            for (long i = 0; i < limit; i++) {
                disp_define_symbol(NULL, var_name, disp_make_long(i), 0);
                disp_val *b = body;
                while (b && T(b) == DISP_CONS) {
                    last_result = disp_eval(NULL, disp_car(b));
                    b = disp_cdr(b);
                }
            }
            // 循环正常结束，取 result 表达式
            if (result_expr)
                last_result = disp_eval(NULL, result_expr);
        }
        // 恢复旧值，清理保护
        if (old_sym) {
            disp_define_symbol(NULL, var_name, old_val ? old_val : NIL, 0);
        } else {
            disp_define_symbol(NULL, var_name, NIL, 0);
        }
        if (old_val != NULL) gc_remove_root(&old_val);
        gc_remove_root(&rest);
        normal_exit = 1;
    } CATCH {
        // 异常：恢复绑定
        if (old_sym) {
            disp_define_symbol(NULL, var_name, old_val ? old_val : NIL, 0);
        } else {
            disp_define_symbol(NULL, var_name, NIL, 0);
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
    DEF("dotimes" , MKB(dotimes_builtin, "<#dotimes>"), 1);
}
