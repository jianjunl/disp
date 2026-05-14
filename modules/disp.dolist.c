
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

// ======================== dolist 循环 ========================
static disp_val* dolist_builtin(disp_scope_t *scope, disp_val *expr) {
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
    disp_val *lst = disp_eval(NULL, list_expr);

    disp_val *old_sym = disp_find_symbol(NULL, var_name);
    disp_val *old_val = old_sym ? disp_get_symbol_value(old_sym) : NULL;
    if (old_val != NULL) gc_add_root(&old_val);

    disp_val * volatile last_result = NIL;
    volatile int normal_exit = 0;
    TRY {
        for (disp_val *p = lst; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
            disp_val *elem = disp_car(p);
            disp_define_symbol(NULL, var_name, elem, 0);
            disp_val *b = body;
            while (b && T(b) == DISP_CONS) {
                last_result = disp_eval(NULL, disp_car(b));
                b = disp_cdr(b);
            }
        }
        if (result_expr)
            last_result = disp_eval(NULL, result_expr);
        // 恢复
        if (old_sym) {
            disp_define_symbol(NULL, var_name, old_val ? old_val : NIL, 0);
        } else {
            disp_define_symbol(NULL, var_name, NIL, 0);
        }
        if (old_val != NULL) gc_remove_root(&old_val);
        gc_remove_root(&rest);
        normal_exit = 1;
    } CATCH {
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
    DEF("dolist"  , MKB(dolist_builtin , "<#dolist>" ), 1);
}
