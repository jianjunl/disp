
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

disp_val* disp_eval_body(disp_val *body) {
    disp_val *result = NIL;
    while (body && T(body) == DISP_CONS) {
        result = disp_eval(disp_car(body));
        body = disp_cdr(body);
    }
    return result;
}

disp_val* disp_eval(disp_val *expr) {
    if (!expr) return NIL;
    gc_add_root(&expr);   // 保护入口表达式
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
        case DISP_SYMBOL:
            if (V(expr) == QUIT) {
                exit(0);
            }
            DBG("eval symbol '%s' -> value %p (type %d)", S(expr), V(expr), V(expr) ? T(V(expr)) : -1);
            return V(expr);
        case DISP_CONS: {
            // Evaluate the function (car) – could be built‑in, closure, or macro
            disp_val *func_val = disp_eval(disp_car(expr));
            if (T(func_val) == DISP_MACRO) {
                disp_val *expansion = disp_expand_macro(func_val, expr);
                if (expansion == NIL) {
                    gc_remove_root(&expr);
                    return NIL;
                }
                gc_add_root(&expansion);       // 保护展开结果
                disp_val *result = disp_eval(expansion);
                gc_remove_root(&expansion);
                gc_remove_root(&expr);
                return result;
            }

            if (T(func_val) == DISP_BUILTIN) {
                gc_remove_root(&expr);
                return disp_get_builtin(func_val)(expr);
            }

            // Collect evaluated arguments
            int arg_count = 0;
            disp_val *arg_list = disp_cdr(expr);
            for (disp_val *a = arg_list; a && T(a) == DISP_CONS; a = disp_cdr(a))
                arg_count++;
            disp_val **args = gc_malloc(arg_count * sizeof(disp_val*));
            gc_add_root(&args);                // 保护数组本身
            int i = 0;
            for (disp_val *a = arg_list; a && T(a) == DISP_CONS; a = disp_cdr(a)) {
                args[i] = disp_eval(disp_car(a));
                gc_add_root(&args[i]);         // 保护每个参数
                i++;
            }

            disp_val *result;
            if (T(func_val) == DISP_SYSCALL) {
                result = disp_get_syscall(func_val)(args, arg_count);
            } else if (T(func_val) == DISP_CLOSURE) {
                result = disp_apply_closure(func_val, args, arg_count);
            } else {
                // 释放保护后报错
                for (int k = 0; k < i; k++) gc_remove_root(&args[k]);
                gc_remove_root(&args);
                gc_free(args);
                char *s = disp_string(func_val);
                ERET(NIL, "%s is not a function or macro", s);
                gc_free(s);
                gc_remove_root(&expr);
                return NIL;
            }
            for (int k = 0; k < i; k++) gc_remove_root(&args[k]);
            gc_remove_root(&args);
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
