
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

// ======================== dolist 循环 ========================
static disp_val dolist_builtin(disp_env_t *env, disp_val expr) {
    disp_val rest = disp_cdr(expr);
    if (N(rest) || T(rest) != FLAG_CONS)
        ERET(NIL, "dolist: missing binding list");
    disp_val binding = disp_car(rest);
    if (T(binding) != FLAG_CONS)
        ERET(NIL, "dolist: malformed binding");
    disp_val var = disp_car(binding);
    if (T(var) != FLAG_SYMBOL)
        ERET(NIL, "dolist: variable must be a symbol");
    disp_sid var_name = SYM_ID(var);
    disp_val list_expr = disp_car(disp_cdr(binding));
    if (N(list_expr))
        ERET(NIL, "dolist: missing list");
    disp_val volatile result_expr = NIL;
    disp_val rest_binding = disp_cdr(disp_cdr(binding));
    if (NN(rest_binding) && T(rest_binding) == FLAG_CONS)
        result_expr = disp_car(rest_binding);
    disp_val body = disp_cdr(rest);

    // 保护整个尾部
    gc_add_root(&rest);

    // 求值列表表达式（在当前作用域中）
    disp_val volatile lst = NIL;
    lst = disp_eval(env, list_expr);
    if (N(lst)) {
        gc_remove_root(&rest);
        ERET(NIL, "dolist: list expression evaluation failed");
    }

    // 创建新作用域（子作用域，父作用域为当前 env）
    disp_env_t *loop_env = disp_new_env(env);
    gc_add_root(&loop_env);

    // 先绑定变量为 NIL（占位）
    disp_define_symbol(loop_env, var_name, NIL, 0);

    disp_val volatile last_result = NIL;
    volatile int normal_exit = 0;
    TRY {
        // 遍历列表
        for (disp_val volatile p = lst; NN(p) && T(p) == FLAG_CONS; p = disp_cdr(p)) {
            disp_val elem = disp_car(p);
            // 更新循环作用域中的变量绑定
            disp_define_symbol(loop_env, var_name, elem, 0);
            // 执行 body（在 loop_env 中）
            disp_val b = body;
            while (NN(b) && T(b) == FLAG_CONS) {
                last_result = disp_eval(loop_env, disp_car(b));
                b = disp_cdr(b);
            }
        }
        // 求值 result 表达式（仍在 loop_env 中，变量最后一次迭代的值仍绑定）
        if (NN(result_expr))
            last_result = disp_eval(loop_env, result_expr);
        else
            last_result = NIL;
        normal_exit = 1;
    } CATCH {
        // 异常：释放保护后传播
        gc_remove_root(&loop_env);
        gc_remove_root(&rest);
        THROW_THROWN();
    }
    END_TRY;

    if (normal_exit) {
        gc_remove_root(&loop_env);
        gc_remove_root(&rest);
        return last_result;
    }
    return NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    REGI(DOLIST , MKB(dolist_builtin , "<#dolist>" ), 1);
}
