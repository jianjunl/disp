
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

static void bind_arguments_to_scope(disp_scope_t *scope, disp_val *params, disp_val **args, int arg_count) {
    // 处理固定参数和 rest 参数（与原来类似，但直接在 scope 上 define）
    int fixed = 0;
    disp_val *rest_sym = NIL;
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) fixed++;
    if (params && T(params) != DISP_CONS) rest_sym = params;

    int idx = 0;
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        const char *name = disp_get_symbol_name(disp_car(p));
        disp_define_symbol(scope, name, args[idx++], 0);
    }
    if (rest_sym != NIL) {
        // 收集剩余参数为列表
        const char *rest_name = disp_get_symbol_name(rest_sym);
        disp_val *rest_list = NIL;
        for (int j = arg_count - 1; j >= fixed; j--) {
            rest_list = disp_make_cons(args[j], rest_list);
        }
        disp_define_symbol(scope, rest_name, rest_list, 0);
    }
}

disp_val* disp_apply_closure(disp_val *closure, disp_val **args, int arg_count) {
    // --- 关键：创建新作用域，父作用域为闭包的环境 ---
    disp_scope_t *new_scope = disp_new_scope(disp_get_closure_env(closure));
    // 绑定形参与实参到新作用域
    bind_arguments_to_scope(new_scope, disp_get_closure_params(closure), args, arg_count);
    // 在新作用域中求值函数体
    return disp_eval_body(new_scope, disp_get_closure_body(closure));
}

// 宏展开时使用定义环境求值宏体
disp_val* disp_expand_macro(disp_scope_t *call_scope, disp_val *macro, disp_val *expr) {
    disp_val *args = disp_cdr(expr);
    disp_val *params = disp_get_closure_params(macro);
    disp_val *body = disp_get_closure_body(macro);
    disp_scope_t *macro_env = disp_get_closure_env(macro);   // 需要新增 getter

    // 创建新作用域：children of macro_env
    disp_scope_t *expand_scope = disp_new_scope(macro_env);
    // 将实际参数（未求值）绑定到 expand_scope
    int arg_count = 0;
    for (disp_val *a = args; a && T(a) == DISP_CONS; a = disp_cdr(a)) arg_count++;
    disp_val **arg_forms = gc_typed_malloc(arg_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    int i = 0;
    for (disp_val *a = args; a && T(a) == DISP_CONS; a = disp_cdr(a)) {
        arg_forms[i] = disp_car(a);
        i++;
    }
    bind_arguments_to_scope(expand_scope, params, arg_forms, arg_count);
    // 在 expand_scope 中求值宏体，得到展开式
    disp_val *expansion = disp_eval_body(expand_scope, body);
    gc_free(arg_forms);
    return expansion;
}
