
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

// Helper: bind parameters to evaluated arguments, return saved bindings
typedef struct {
    char *name;
    disp_val *old_value;
    int existed;   /* 1 if the symbol existed before binding */
} saved_binding;

static saved_binding* bind_params(disp_val *params, disp_val **args, int arg_count) {
    // Count fixed parameters and detect rest symbol
    int fixed_count = 0;
    disp_val *rest_sym = NIL;
    disp_val *p = params;
    while (p && T(p) == DISP_CONS) {
        fixed_count++;
        p = disp_cdr(p);
    }
    if (p && p != NIL) {
        // p is a symbol (the rest parameter)
        rest_sym = p;
    }

    // Validate argument count
    if (rest_sym == NIL) {
        if (fixed_count != arg_count) {
            ERRO("lambda: argument count mismatch (expected %d, got %d)\n", fixed_count, arg_count);
            return NULL;
        }
    } else {
        if (arg_count < fixed_count) {
            ERRO("lambda: too few arguments (need at least %d)\n", fixed_count);
            return NULL;
        }
    }

    // Allocate saved bindings: fixed parameters + optionally rest
    int total_bindings = fixed_count + (rest_sym != NIL ? 1 : 0);
    saved_binding *saved = gc_malloc(total_bindings * sizeof(saved_binding));
    if (!saved) return NULL;

    int i = 0;
    p = params;
    // Bind fixed parameters
    while (p && T(p) == DISP_CONS) {
        disp_val *sym = disp_car(p);
        if (T(sym) != DISP_SYMBOL) {
            gc_free(saved);
            ERRO("lambda: parameter not a symbol\n");
            return NULL;
        }
        const char *name = disp_get_symbol_name(sym);
        disp_val *old_sym = disp_find_symbol(name);
        saved[i].name = gc_strdup(name);
        saved[i].existed = (old_sym != NULL);
        saved[i].old_value = saved[i].existed ? disp_get_symbol_value(old_sym) : NIL;
        /* 将旧值注册为 GC 精确根，防止被回收 */
        if (saved[i].existed && saved[i].old_value != NULL) {
            gc_add_root(&saved[i].old_value);
        }
        disp_define_symbol(name, args[i], 0);
        i++;
        p = disp_cdr(p);
    }

    // Bind rest parameter if present
    if (rest_sym != NIL) {
        const char *rest_name = disp_get_symbol_name(rest_sym);
        // Collect remaining arguments into a list
        disp_val *rest_list = NIL;
        for (int j = arg_count - 1; j >= fixed_count; j--) {
            rest_list = disp_make_cons(args[j], rest_list);
        }
        // 临时保护 rest_list，防止在绑定前被 GC 回收
        gc_add_root(&rest_list);

        disp_val *old_rest_sym = disp_find_symbol(rest_name);
        saved[i].name = gc_strdup(rest_name);
        saved[i].existed = (old_rest_sym != NULL);
        saved[i].old_value = saved[i].existed ? disp_get_symbol_value(old_rest_sym) : NIL;
        if (saved[i].existed && saved[i].old_value != NULL) {
            gc_add_root(&saved[i].old_value);
        }
        disp_define_symbol(rest_name, rest_list, 0);
        // 绑定完成，rest_list 现在由符号表持有，可解除保护
        gc_remove_root(&rest_list);
    }

    return saved;
}

static void restore_bindings(saved_binding *saved, int count) {
    for (int i = 0; i < count; i++) {
        /* 移除旧值的根保护 */
        if (saved[i].existed && saved[i].old_value != NULL) {
            gc_remove_root(&saved[i].old_value);
        }
        if (saved[i].existed)
            disp_define_symbol(saved[i].name, saved[i].old_value, 0);
        else
            disp_define_symbol(saved[i].name, NIL, 0);
            //disp_remove_symbol(saved[i].name);
        gc_free(saved[i].name);
    }
    gc_free(saved);
}

disp_val* disp_apply_closure(disp_val *closure, disp_val **args, int arg_count) {
    disp_val *params = disp_get_closure_params(closure);
    disp_val *body = disp_get_closure_body(closure);
    
    // 如果没有参数且参数列表为空，直接执行 body
    if (arg_count == 0 && params == NIL) {
        return disp_eval_body(body);
    }
    
    saved_binding *saved = bind_params(params, args, arg_count);
    if (!saved) return NIL;
    // 计算参数个数（用于恢复）
    int param_count = 0;
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) param_count++;
    if (params != NIL && T(params) != DISP_CONS) param_count++; // rest param case
    disp_val *result = disp_eval_body(body);
    restore_bindings(saved, param_count);
    return result;
}

disp_val* disp_expand_macro(disp_val *macro, disp_val *expr) {
    disp_val *args = disp_cdr(expr);                     // unevaluated arguments
    disp_val *params = disp_get_closure_params(macro);
    disp_val *body = disp_get_closure_body(macro);

    // Count fixed parameters and detect rest symbol
    int fixed_count = 0;
    disp_val *p = params;
    while (p && T(p) == DISP_CONS) {
        fixed_count++;
        p = disp_cdr(p);
    }
    int has_rest = (p != NULL && p != NIL);   // p is a symbol (the rest parameter)

    // Count actual arguments
    int arg_count = 0;
    for (disp_val *a = args; a && T(a) == DISP_CONS; a = disp_cdr(a))
        arg_count++;

    // Validate argument count
    if (has_rest) {
        if (arg_count < fixed_count) {
            ERRO("macro: too few arguments (need at least %d)\n", fixed_count);
            return NIL;
        }
    } else {
        if (fixed_count != arg_count) {
            ERRO("macro: argument count mismatch\n");
            return NIL;
        }
    }

    // Collect all argument forms into an array
    disp_val **arg_forms = gc_malloc(arg_count * sizeof(disp_val*));
    int i = 0;
    for (disp_val *a = args; a && T(a) == DISP_CONS; a = disp_cdr(a)) {
        arg_forms[i] = disp_car(a);
        // 临时保护表达式节点，防止 GC 回收
        gc_add_root(&arg_forms[i]);
        i++;
    }

    // Bind parameters (including rest)
    saved_binding *saved = bind_params(params, arg_forms, arg_count);
    if (!saved) {
        // 出错时移除保护
        for (int idx = 0; idx < i; idx++) gc_remove_root(&arg_forms[idx]);
        gc_free(arg_forms);
        return NIL;
    }

    // Expand the macro body
    disp_val *expansion = disp_eval_body(body);
    // 保护展开结果
    gc_add_root(&expansion);

    // Restore bindings (total = fixed parameters + optional rest)
    int total_bindings = fixed_count + (has_rest ? 1 : 0);
    restore_bindings(saved, total_bindings);
    // 移除表达式保护
    for (int idx = 0; idx < arg_count; idx++) {
        gc_remove_root(&arg_forms[idx]);
    }
    gc_free(arg_forms);
    return expansion;
}
