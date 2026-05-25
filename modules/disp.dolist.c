
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

// ======================== dolist 循环 ========================
static disp_box dolist_builtin(disp_scope_t *scope, disp_box expr) {
    disp_box rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "dolist: missing binding list");
    disp_box binding = disp_car(rest);
    if (T(binding) != DISP_CONS)
        ERET(NIL, "dolist: malformed binding");
    disp_box var = disp_car(binding);
    if (T(var) != DISP_SYMBOL)
        ERET(NIL, "dolist: variable must be a symbol");
    const char *var_name = disp_get_symbol_name(var);
    disp_box list_expr = disp_car(disp_cdr(binding));
    if (!list_expr)
        ERET(NIL, "dolist: missing list");
    disp_box result_expr = NIL;
    disp_box rest_binding = disp_cdr(disp_cdr(binding));
    if (rest_binding && T(rest_binding) == DISP_CONS)
        result_expr = disp_car(rest_binding);
    disp_box body = disp_cdr(rest);

    // 保护整个尾部
    gc_add_root(&rest);

    // 求值列表表达式（在当前作用域中）
    disp_box lst = disp_eval(scope, list_expr);
    if (!lst) {
        gc_remove_root(&rest);
        ERET(NIL, "dolist: list expression evaluation failed");
    }

    // 创建新作用域（子作用域，父作用域为当前 scope）
    disp_scope_t *loop_scope = disp_new_scope(scope);
    gc_add_root(&loop_scope);

    // 先绑定变量为 NIL（占位）
    disp_define_symbol(loop_scope, var_name, NIL, 0);

    disp_box  volatile last_result = NIL;
    volatile int normal_exit = 0;
    TRY {
        // 遍历列表
        for (disp_box  volatile p = lst; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
            disp_box elem = disp_car(p);
            // 更新循环作用域中的变量绑定
            disp_define_symbol(loop_scope, var_name, elem, 0);
            // 执行 body（在 loop_scope 中）
            disp_box b = body;
            while (b && T(b) == DISP_CONS) {
                last_result = disp_eval(loop_scope, disp_car(b));
                b = disp_cdr(b);
            }
        }
        // 求值 result 表达式（仍在 loop_scope 中，变量最后一次迭代的值仍绑定）
        if (result_expr)
            last_result = disp_eval(loop_scope, result_expr);
        else
            last_result = NIL;
        normal_exit = 1;
    } CATCH {
        // 异常：释放保护后传播
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
    DEF("dolist"  , MKB(dolist_builtin , "<#dolist>" ), 1);
}
