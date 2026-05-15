
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <ucontext.h>
#include <alloca.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

/* ======================== Funcs ======================== */

union disp_data {
    /* 闭包 / 宏 */
    struct {
        disp_val *params;
        disp_val *body;
        disp_scope_t *env;
    } closure;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, closure.params),
    GC_OFF(disp_data, closure.body),
    GC_OFF(disp_data, closure.env)
);

static void intern_params(disp_scope_t *env, disp_val *params) {
    for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        disp_val *sym = disp_car(p);
        if (T(sym) == DISP_SYMBOL) {
            const char *name = disp_get_symbol_name(sym);
            if (!disp_find_symbol(env, name)) {
                disp_define_symbol(env, name, NIL, 0);
            }
        }
    }
}

disp_val* disp_make_closure(disp_scope_t *env, disp_val *params, disp_val *body) {
    intern_params(env, params);
    disp_val *v = DISP_ALLOC_TI(DISP_CLOSURE);
    v->data->closure.params = params;
    v->data->closure.body   = body;
    v->data->closure.env    = env;
    return v;
}

disp_val* disp_make_macro(disp_scope_t *env, disp_val *params, disp_val *body) {
    intern_params(env, params);
    disp_val *v = DISP_ALLOC_TI(DISP_MACRO);
    v->data->closure.params = params;
    v->data->closure.body   = body;
    v->data->closure.env    = env;
    return v;
}

disp_val* disp_get_closure_params(disp_val *closure) {
    if (T(closure) != DISP_CLOSURE && T(closure) != DISP_MACRO) {
        ERRO("disp_get_closure_params: not a closure/macro\n");
        return NIL;
    }
    return closure->data->closure.params;
}

disp_val* disp_get_closure_body(disp_val *closure) {
    if (T(closure) != DISP_CLOSURE && T(closure) != DISP_MACRO) {
        ERRO("disp_get_closure_body: not a closure/macro\n");
        return NIL;
    }
    return closure->data->closure.body;
}

disp_scope_t* disp_get_closure_env(disp_val *closure) {
    if (T(closure) != DISP_CLOSURE && T(closure) != DISP_MACRO) {
        ERRO("disp_get_closure_env: not a closure/macro\n");
        return NULL;
    }
    return closure->data->closure.env;
}

void bind_arguments_to_scope(disp_scope_t *scope, disp_val *params, disp_val **args, int arg_count) {
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
    disp_scope_t *env = disp_get_closure_env(closure);
    disp_val *params = disp_get_closure_params(closure);
    disp_val *body   = disp_get_closure_body(closure);

    /* 优化：若闭包的环境是全局作用域（没有捕获任何非全局变量），
       则使用栈上分配的临时帧来绑定参数，避免堆分配作用域链。 */
    if (env == disp_global_scope) {
        /* 统计固定参数个数及 rest 参数 */
        int fixed = 0;
        disp_val *rest_sym = NIL;
        for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) fixed++;
        if (params && T(params) != DISP_CONS) rest_sym = params;

        int bind_count = fixed + (rest_sym != NIL ? 1 : 0);
        /* 分配临时帧（使用 malloc，实际生产可改用 alloca 以进一步提高速度） */
        size_t frame_size = sizeof(disp_frame_t) + bind_count * sizeof(disp_frame_binding_t);
        disp_frame_t *frame = (disp_frame_t*)alloca(frame_size);
        //disp_frame_t *frame = (disp_frame_t*)malloc(frame_size);
        if (!frame) {
            ERRO("disp_apply_closure: failed to allocate frame");
            return NIL;
        }
        frame->prev = disp_current_frame;
        frame->binding_count = bind_count;

        int idx = 0;
        /* 固定参数绑定 */
        for (disp_val *p = params; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
            const char *name = disp_get_symbol_name(disp_car(p));
            frame->bindings[idx].name = (char*)name;
            frame->bindings[idx].value = args[idx];
            idx++;
        }
        /* rest 参数绑定 */
        if (rest_sym != NIL) {
            const char *rest_name = disp_get_symbol_name(rest_sym);
            disp_val *rest_list = NIL;
            for (int j = arg_count - 1; j >= fixed; j--) {
                rest_list = disp_make_cons(args[j], rest_list);
            }
            frame->bindings[idx].name = (char*)rest_name;
            frame->bindings[idx].value = rest_list;
        }

        disp_current_frame = frame;
        disp_val *result = disp_eval_body(env, body);  /* 注意：此处环境为全局作用域，函数体内的自由变量将通过帧栈查找 */
        disp_current_frame = frame->prev;

        //free(frame);
        return result;
    } else {
        /* 回退到原有的堆作用域方式（适用于捕获了非全局变量的闭包） */
        disp_scope_t *new_scope = disp_new_scope(env);
        bind_arguments_to_scope(new_scope, params, args, arg_count);
        return disp_eval_body(new_scope, body);
    }
}

/*
disp_val* disp_apply_closure(disp_val *closure, disp_val **args, int arg_count) {
    // --- 关键：创建新作用域，父作用域为闭包的环境 ---
    disp_scope_t *new_scope = disp_new_scope(disp_get_closure_env(closure));
    // 绑定形参与实参到新作用域
    bind_arguments_to_scope(new_scope, disp_get_closure_params(closure), args, arg_count);
    // 在新作用域中求值函数体
    return disp_eval_body(new_scope, disp_get_closure_body(closure));
}
*/
