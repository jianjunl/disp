
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
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

/* ======================== Evaluator ======================== */

disp_val* disp_eval_body(disp_scope_t *scope, disp_val *body) {
    disp_val *result = NIL;
    while (body && T(body) == DISP_CONS) {
        result = disp_eval(scope, disp_car(body));
        body = disp_cdr(body);
    }
    return result;
}

extern disp_scope_t *global_scope;

disp_val* disp_eval(disp_scope_t *scope, disp_val *expr) {
    if (!expr) return NIL;
    gc_add_root(&expr);   // 保护入口表达式
    if (!scope) scope = global_scope;
    switch (T(expr)) {
        case DISP_BYTE:
        case DISP_SHORT:
        case DISP_INT:
        case DISP_LONG:
        case DISP_FLOAT:
        case DISP_DOUBLE:
        case DISP_STRING:
        case DISP_VOID:
            gc_remove_root(&expr);
            return expr;
        case DISP_SYMBOL: {
            if (V(expr) == QUIT) {
                exit(0);
            }
            disp_val *val = disp_find_symbol(scope, S(expr));
            if (!val || val == NIL) {
                ERET(NIL, "undefined symbol: %s", S(expr));
            }
            disp_val *value = V(val);   // 注意：需要查找后取其 value
            gc_remove_root(&expr);
            return value;
        }
        case DISP_CONS: {
            // Evaluate the function (car) – could be built‑in, closure, or macro
            disp_val *func_val = disp_eval(scope, disp_car(expr));  // 函数、宏也是表达式
            // 宏展开
            if (T(func_val) == DISP_MACRO) {
                disp_val *expansion = disp_expand_macro(func_val, expr);
                if (expansion == NIL) {
                    gc_remove_root(&expr);
                    return NIL;
                }
                disp_val *result = disp_eval(scope, expansion);
                gc_remove_root(&expr);
                return result;
            }

            if (T(func_val) == DISP_BUILTIN) {
                gc_remove_root(&expr);
                return disp_get_builtin(func_val)(scope, expr);
            }

            // Collect evaluated arguments
            int arg_count = 0;
            disp_val *arg_list = disp_cdr(expr);
            for (disp_val *a = arg_list; a && T(a) == DISP_CONS; a = disp_cdr(a))
                arg_count++;
            disp_val **args = gc_typed_malloc(arg_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
            int i = 0;
            for (disp_val *a = arg_list; a && T(a) == DISP_CONS; a = disp_cdr(a)) {
                args[i++] = disp_eval(scope, disp_car(a));
            }

            disp_val *result;
            if (T(func_val) == DISP_CLOSURE) {
                result = disp_apply_closure(func_val, args, arg_count);
            } else if (T(func_val) == DISP_SYSCALL) {
                result = disp_get_syscall(func_val)(args, arg_count);
            } else {
                gc_free(args);
                char *s = disp_string(func_val);
                ERET(NIL, "%s is not a function or macro", s);
                gc_free(s);
                gc_remove_root(&expr);
                return NIL;
            }
            gc_free(args);
            gc_remove_root(&expr);
            return result;
        }
        default:
            gc_remove_root(&expr);
            ERET(NIL, "cannot evaluate");
    }
    gc_remove_root(&expr);
    return NIL;   // never reach
}
