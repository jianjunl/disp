
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

extern void bind_arguments_to_scope(disp_scope_t *scope, disp_val *params, disp_val **args, int arg_count);

static disp_val* letf(disp_scope_t *scope, disp_val *expr) {
    // 解析命名 let 语法
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "named let: missing name");
    disp_val *name = disp_car(rest);
    if (T(name) != DISP_SYMBOL) ERET(NIL, "named let: name must be a symbol");
    
    disp_val *bindings_rest = disp_cdr(rest);
    if (!bindings_rest || T(bindings_rest) != DISP_CONS) ERET(NIL, "named let: missing bindings");
    disp_val *bindings = disp_car(bindings_rest);
    disp_val *body = disp_cdr(bindings_rest);
    if (!body) ERET(NIL, "named let: missing body");
    
    // 提取变量和初值
    int var_count = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b)) var_count++;
    disp_val **var_syms = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    disp_val **init_exprs = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    //gc_add_root(&var_syms); gc_add_root(&init_exprs);
    int idx = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b)) {
        disp_val *pair = disp_car(b);
        if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
            //gc_remove_root(&init_exprs); gc_remove_root(&var_syms);
            gc_free(init_exprs); gc_free(var_syms);
            ERET(NIL, "named let: malformed binding");
        }
        var_syms[idx] = disp_car(pair);
        init_exprs[idx] = disp_car(disp_cdr(pair));
        idx++;
        b = disp_cdr(b);
    }
    
    // 创建循环作用域（父作用域为当前 scope）
    disp_scope_t *loop_scope = disp_new_scope(scope);
    //gc_add_root(&loop_scope);
    
    // 计算初值并绑定到循环变量
    for (int i = 0; i < var_count; i++) {
        disp_val *init_val = disp_eval(scope, init_exprs[i]);
        const char *vname = disp_get_symbol_name(var_syms[i]);
        disp_define_symbol(loop_scope, vname, init_val, 0);
    }
    
    // 创建闭包：参数列表就是这些变量符号，body 是原 body
    disp_val *params = NIL;
    for (int i = var_count - 1; i >= 0; i--) params = disp_make_cons(var_syms[i], params);
    disp_val *closure = disp_make_closure(loop_scope, params, body, 1);  // 直接创建带 reuse_scope 的闭包
    disp_define_symbol(loop_scope, S(name), closure, 0);
    
    // 准备参数（当前循环变量的值）
    disp_val **args = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    //gc_add_root(&args);
    for (int i = 0; i < var_count; i++) {
        const char *vname = disp_get_symbol_name(var_syms[i]);
        disp_val *sym = disp_find_symbol(loop_scope, vname);
        args[i] = disp_get_symbol_value(sym);
    }
    
    // 调用闭包，执行 body（由于闭包设置了 reuse_scope，它会直接在 loop_scope 中更新绑定，而不会创建新作用域）
    disp_val *result = disp_apply_closure(closure, args, var_count);
    
    // 清理
    //gc_remove_root(&args);
    gc_free(args);
    //gc_remove_root(&loop_scope);
    //gc_remove_root(&var_syms); gc_remove_root(&init_exprs);
    gc_free(var_syms); gc_free(init_exprs);
    
    return result;
}

static disp_val* let_builtin(disp_scope_t *scope, disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "let: missing binding list");

    disp_val *first = disp_car(rest);
    if (T(first) == DISP_SYMBOL) {
        disp_val *second_rest = disp_cdr(rest);
        if (second_rest && T(second_rest) == DISP_CONS)
            return letf(scope, expr);
    }

    disp_val *bindings = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body)
        ERET(NIL, "let: missing body");

    int var_count = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b))
        var_count++;

    if (var_count == 0)
        return disp_eval_body(scope, body);

    /* 提取变量符号和初值表达式 */
    disp_val **var_syms = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    disp_val **init_exprs = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    gc_add_root(&var_syms);
    gc_add_root(&init_exprs);

    int idx = 0;
    disp_val *b = bindings;
    while (b && T(b) == DISP_CONS) {
        disp_val *pair = disp_car(b);
        if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
            gc_remove_root(&init_exprs);
            gc_remove_root(&var_syms);
            gc_free(init_exprs);
            gc_free(var_syms);
            ERET(NIL, "let: malformed binding (var expr)");
        }
        var_syms[idx] = disp_car(pair);
        init_exprs[idx] = disp_car(disp_cdr(pair));
        idx++;
        b = disp_cdr(b);
    }

    /* 创建新作用域，父作用域为当前 scope */
    disp_scope_t *new_scope = disp_new_scope(scope);
    gc_add_root(&new_scope);

    disp_val *result = NIL;

    if (disp_car(expr) == LETA) {   /* let* : 顺序绑定 */
        for (int i = 0; i < var_count; i++) {
            disp_val *val = disp_eval(new_scope, init_exprs[i]);
            /* 立即绑定到新作用域 */
            const char *name = disp_get_symbol_name(var_syms[i]);
            disp_define_symbol(new_scope, name, val, 0);
        }
        result = disp_eval_body(new_scope, body);
    } else {                         /* let : 并行绑定 */
        /* 在当前作用域中计算所有初值 */
        disp_val **values = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
        gc_add_root(&values);
        for (int i = 0; i < var_count; i++) {
            values[i] = disp_eval(scope, init_exprs[i]);
            gc_add_root(&values[i]);
        }
        /* 构造参数列表（符号列表） */
        disp_val *params = NIL;
        for (int i = var_count - 1; i >= 0; i--)
            params = disp_make_cons(var_syms[i], params);
        /* 一次性绑定到新作用域 */
        bind_arguments_to_scope(new_scope, params, values, var_count);
        /* 执行 body */
        result = disp_eval_body(new_scope, body);
        /* 清理 values 的保护 */
        for (int i = 0; i < var_count; i++)
            gc_remove_root(&values[i]);
        gc_remove_root(&values);
        gc_free(values);
    }

    /* 清理资源 */
    gc_remove_root(&new_scope);
    gc_remove_root(&var_syms);
    gc_remove_root(&init_exprs);
    gc_free(var_syms);
    gc_free(init_exprs);

    return result;
}

static disp_val* letrec_builtin(disp_scope_t *scope, disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "letrec: missing binding list");

    disp_val *first = disp_car(rest);
    if (T(first) == DISP_SYMBOL) {
        disp_val *second_rest = disp_cdr(rest);
        if (second_rest && T(second_rest) == DISP_CONS)
            return letf(scope, expr);
    }

    disp_val *bindings = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body)
        ERET(NIL, "letrec: missing body");

    int var_count = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b))
        var_count++;

    if (var_count == 0)
        return disp_eval_body(scope, body);

    disp_val **var_syms = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    disp_val **init_exprs = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    gc_add_root(&var_syms);
    gc_add_root(&init_exprs);

    int idx = 0;
    disp_val *b = bindings;
    while (b && T(b) == DISP_CONS) {
        disp_val *pair = disp_car(b);
        if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
            gc_remove_root(&init_exprs);
            gc_remove_root(&var_syms);
            gc_free(init_exprs);
            gc_free(var_syms);
            ERET(NIL, "letrec: malformed binding (var expr)");
        }
        var_syms[idx] = disp_car(pair);
        init_exprs[idx] = disp_car(disp_cdr(pair));
        idx++;
        b = disp_cdr(b);
    }

    /* 创建新作用域 */
    disp_scope_t *new_scope = disp_new_scope(scope);
    gc_add_root(&new_scope);

    /* 先在新作用域中绑定占位符 NIL */
    for (int i = 0; i < var_count; i++) {
        const char *name = disp_get_symbol_name(var_syms[i]);
        disp_define_symbol(new_scope, name, NIL, 0);
    }

    /* 计算所有初值（在新作用域中，此时变量已存在但值为 NIL） */
    disp_val **values = gc_typed_malloc(var_count * sizeof(disp_val*), &GC_TYPE_PTR_ARRAY);
    gc_add_root(&values);
    if (disp_car(expr) == LETRECA) {   /* letrec* : 顺序初始化 */
        for (int i = 0; i < var_count; i++) {
            values[i] = disp_eval(new_scope, init_exprs[i]);
            gc_add_root(&values[i]);
            /* 立即更新绑定 */
            const char *name = disp_get_symbol_name(var_syms[i]);
            disp_define_symbol(new_scope, name, values[i], 0);
        }
    } else {                           /* letrec : 并行初始化 */
        for (int i = 0; i < var_count; i++) {
            values[i] = disp_eval(new_scope, init_exprs[i]);
            gc_add_root(&values[i]);
        }
        for (int i = 0; i < var_count; i++) {
            const char *name = disp_get_symbol_name(var_syms[i]);
            disp_define_symbol(new_scope, name, values[i], 0);
        }
    }

    /* 执行 body */
    disp_val *result = disp_eval_body(new_scope, body);

    /* 清理 */
    for (int i = 0; i < var_count; i++)
        gc_remove_root(&values[i]);
    gc_remove_root(&values);
    gc_free(values);
    gc_remove_root(&new_scope);
    gc_remove_root(&var_syms);
    gc_remove_root(&init_exprs);
    gc_free(var_syms);
    gc_free(init_exprs);

    return result;
}

/* 模块初始化函数 */
void disp_init_module(void) {
    DEF("let"     , MKB(let_builtin    , "<#let>"    ), 1);
    DEF("let*"    , MKB(let_builtin    , "<#let*>"   ), 1);
    DEF("letrec"  , MKB(letrec_builtin , "<#letrec>" ), 1);
    DEF("letrec*" , MKB(letrec_builtin , "<#letrec*>"), 1);
}
