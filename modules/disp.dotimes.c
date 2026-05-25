
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

extern void bind_arguments_to_scope(disp_scope_t *scope, disp_box params, disp_box *args, int arg_count);

// ======================== dotimes 循环 ========================
static disp_box dotimes_builtin(disp_scope_t *scope, disp_box expr) {
    disp_box rest = disp_cdr(expr);
    if (!rest || T(rest) != FLAG_CONS)
        ERET(NIL, "dotimes: missing binding list");
    disp_box binding = disp_car(rest);
    if (T(binding) != FLAG_CONS)
        ERET(NIL, "dotimes: malformed binding");
    disp_box var = disp_car(binding);
    if (T(var) != FLAG_SYMBOL)
        ERET(NIL, "dotimes: variable must be a symbol");
    const char *var_name = disp_get_symbol_name(var);
    disp_box count_expr = disp_car(disp_cdr(binding));
    if (!count_expr)
        ERET(NIL, "dotimes: missing count");
    disp_box result_expr = NIL;
    disp_box rest_binding = disp_cdr(disp_cdr(binding));
    if (rest_binding && T(rest_binding) == FLAG_CONS)
        result_expr = disp_car(rest_binding);
    disp_box body = disp_cdr(rest);

    // 保护尾部
    gc_add_root(&rest);

    // 求值 count（在当前作用域中）
    disp_box count_val = disp_eval(scope, count_expr);
    long limit;
    int ct = T(count_val);
    if (ct == FLAG_INT) {
        limit = disp_get_int(count_val);
    } else if (ct == FLAG_LONG) {
        limit = disp_get_long(count_val);
    } else {
        gc_remove_root(&rest);
        ERET(NIL, "dotimes: count must be an integer");
    }

    // 创建新作用域
    disp_scope_t *loop_scope = disp_new_scope(scope);
    gc_add_root(&loop_scope);

    // 先绑定变量为 NIL（占位）
    disp_define_symbol(loop_scope, var_name, NIL, 0);

    disp_box  volatile last_result = NIL;
    volatile int normal_exit = 0;
    TRY {
        if (limit <= 0) {
            // 直接求值 result 表达式（如果有）
            if (result_expr)
                last_result = disp_eval(loop_scope, result_expr);
            else
                last_result = NIL;
        } else {
            for (long i = 0; i < limit; i++) {
                disp_box i_val = disp_make_long(i);
                // 更新循环作用域中的变量绑定
                disp_define_symbol(loop_scope, var_name, i_val, 0);
                // 执行 body
                disp_box b = body;
                while (b && T(b) == FLAG_CONS) {
                    last_result = disp_eval(loop_scope, disp_car(b));
                    b = disp_cdr(b);
                }
            }
            // 循环结束后，result 表达式（变量值为最后一次迭代的值，即 limit-1）
            if (result_expr)
                last_result = disp_eval(loop_scope, result_expr);
        }
        normal_exit = 1;
    } CATCH {
        gc_remove_root(&loop_scope);
        gc_remove_root(&rest);
        THROW_THROWN();
    }
    END_TRY;

    if (normal_exit) {
        gc_remove_root(&loop_scope);
        gc_remove_root(&rest);
        return last_result;
    }
    return NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("dotimes" , MKB(dotimes_builtin, "<#dotimes>"), 1);
}
