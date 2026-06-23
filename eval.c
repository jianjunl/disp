
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <ucontext.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

/* ======================== Evaluator ======================== */

// 宏展开时使用定义环境求值宏体
static disp_val expand_macro(disp_val macro, disp_val expr) {
    disp_val args = disp_cdr(expr);
    disp_val params = disp_get_closure_params(macro);
    disp_val body = disp_get_closure_body(macro);
    disp_env_t *macro_env = disp_get_closure_env(macro);

    // 创建新作用域：children of macro_env
    GC_ROOT(disp_env_t, expand_env) = disp_new_env(macro_env);
    // 将实际参数（未求值）绑定到 expand_env
    int arg_count = 0;
    for (disp_val a = args; NN(a) && T(a) == FLAG_CONS; a = disp_cdr(a)) arg_count++;
    disp_val *arg_forms = gc_typed_malloc(arg_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    int i = 0;
    for (disp_val a = args; NN(a) && T(a) == FLAG_CONS; a = disp_cdr(a)) {
        arg_forms[i] = disp_car(a);
        i++;
    }
    bind_arguments_to_env(expand_env, params, arg_forms, arg_count);
    // 在 expand_env 中求值宏体，得到展开式
    disp_val expansion = disp_eval_body(expand_env, body);
    gc_free(arg_forms);
    return expansion;
}

disp_val disp_eval_body(disp_env_t *env, disp_val body) {
    GC_ROOT_AUTO(body);
    disp_val result = NIL;
    while (NN(body) && T(body) == FLAG_CONS) {
        result = disp_eval(env, disp_car(body));
        body = disp_cdr(body);
    }
    return result;
}

disp_val disp_eval(disp_env_t *env, disp_val expr) {
    if (N(expr)) return NIL;
    switch (T(expr)) {
        case FLAG_TYPE:
        case FLAG_BYTE:
        case FLAG_SHORT:
        case FLAG_INT:
        case FLAG_LONG:
        case TAG_LONG:
        case FLAG_FLOAT:
        case FLAG_DOUBLE:
        case FLAG_STRING:
        case FLAG_VOID:
        case TAG_ARGS:
            return expr;
        case FLAG_SYMBOL: {
            if (E(SYM_VALUE(expr), QUIT)) {
                exit(0);
            }
            disp_val val = disp_find_symbol(env, SYM_ID(expr));
            if (N(val) || E(val, NIL)) {
                //ERET(NIL, "undefined symbol: %s", SYM_NAME(expr));
                return NIL;
            }
            disp_val value = SYM_VALUE(val);   // 注意：需要查找后取其 value
            return value;
        }
        case FLAG_CONS: {
            // Evaluate the function (car) – could be built‑in, closure, or macro
            disp_val func_val = disp_eval(env, disp_car(expr));  // 函数、宏也是表达式
            // 宏展开
            if (T(func_val) == FLAG_MACRO) {
                disp_val expansion = expand_macro(func_val, expr);
                if (E(expansion, NIL)) {
                    return NIL;
                }
                return disp_eval(env, expansion);
            }

            if (T(func_val) == FLAG_BUILTIN) {
                return disp_get_builtin(func_val)(env, expr);
            }

            // Collect evaluated arguments
            int arg_count = 0;
            disp_val arg_list = disp_cdr(expr);
            for (disp_val a = arg_list; NN(a) && T(a) == FLAG_CONS; a = disp_cdr(a))
                arg_count++;
            disp_val *args = gc_typed_malloc(arg_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
            int i = 0;
            for (disp_val a = arg_list; NN(a) && T(a) == FLAG_CONS; a = disp_cdr(a)) {
                args[i++] = disp_eval(env, disp_car(a));
            }

            disp_val result;
            if (T(func_val) == FLAG_CLOSURE) {
                result = disp_apply_closure(func_val, args, arg_count);
            } else if (T(func_val) == FLAG_SYSCALL) {
                result = disp_get_syscall(func_val)(args, arg_count);
            } else {
                gc_free(args);
                char *s = disp_string(func_val);
                ERET(NIL, "%s is not a function or macro", s);
                gc_free(s);
                return NIL;
            }
            gc_free(args);
            return result;
        }
        default:
            ERET(NIL, "cannot evaluate");
    }
    return NIL;   // never reach
}
