
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

/*
最终修改的关键点是让 letf 生成 (letrec* ((name lambda)) (name init ...)) 结构，
直接在 letrec* 内部调用函数，确保递归调用时绑定有效。
后续继续扩展 Lisp 特性（比如实现 do、loop 等宏，或增加模式匹配等）
*/
// 处理命名 let 语法: (let name ((var init) ...) body ...)
static disp_val* letf(disp_val *expr) {
    // expr 为 (let name bindings body...)
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "named let: missing name");
    disp_val *name = disp_car(rest);
    if (T(name) != DISP_SYMBOL)
        ERET(NIL, "named let: name must be a symbol");
    
    disp_val *bindings_rest = disp_cdr(rest);
    if (!bindings_rest || T(bindings_rest) != DISP_CONS)
        ERET(NIL, "named let: missing bindings");
    disp_val *bindings = disp_car(bindings_rest);
    if (T(bindings) != DISP_CONS && bindings != NIL)
        ERET(NIL, "named let: bindings must be a list");
    
    disp_val *body = disp_cdr(bindings_rest);
    if (!body)
        ERET(NIL, "named let: missing body");
    
    // 获取 letrec* 符号（确保存在）
    disp_val *letrec_star = disp_find_symbol("letrec*");
    if (!letrec_star || letrec_star == NIL)
        ERET(NIL, "named let: letrec* not defined (load disp.lamb.so)");
    
    // 构造参数列表和参数名
    int var_count = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b))
        var_count++;
    
    // 构造 (lambda (var1 var2 ...) body ...)
    disp_val *params = NIL;
    disp_val **param_list = gc_malloc(var_count * sizeof(disp_val*));
    int i = 0;
    disp_val *b = bindings;
    while (b && T(b) == DISP_CONS) {
        disp_val *pair = disp_car(b);
        if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
            gc_free(param_list);
            ERET(NIL, "named let: malformed binding (var init)");
        }
        param_list[i] = disp_car(pair);  // 变量符号
        i++;
        b = disp_cdr(b);
    }
    // 将参数列表构建为链式 cons
    for (int j = var_count - 1; j >= 0; j--) {
        params = disp_make_cons(param_list[j], params);
    }
    gc_free(param_list);
    
    // 构建 lambda 体
    disp_val *lambda = disp_make_cons(LAMBDA, disp_make_cons(params, body));
    
    // 构造 letrec* 绑定: ((name lambda))
    disp_val *binding = disp_make_cons(
        disp_make_cons(name, disp_make_cons(lambda, NIL)),
        NIL
    );
    
    // 收集初始值表达式（保持顺序）
    disp_val *init_exprs = NIL;
    b = bindings;
    for (int j = 0; j < var_count; j++) {
        disp_val *pair = disp_car(b);
        disp_val *init_expr = disp_car(disp_cdr(pair));
        init_exprs = disp_make_cons(init_expr, init_exprs);
        b = disp_cdr(b);
    }
    // 反转回原始顺序
    disp_val *call_args = NIL;
    for (disp_val *p = init_exprs; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        call_args = disp_make_cons(disp_car(p), call_args);
    }
    
    // 构造 (letrec* ((name lambda)) name)
    disp_val *call_expr = disp_make_cons(name, call_args);
    disp_val *letrec_expr = disp_make_cons(letrec_star,
                           disp_make_cons(binding,
                           disp_make_cons(call_expr, NIL)));
    return disp_eval(letrec_expr);
}

static disp_val* let_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "let: missing binding list");

    disp_val *first = disp_car(rest);
    if (T(first) == DISP_SYMBOL) {
        disp_val *second_rest = disp_cdr(rest);
        if (second_rest && T(second_rest) == DISP_CONS)
            return letf(expr);
    }

    disp_val *bindings = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body) ERET(NIL, "let: missing body");

    // === GC 根保护：防止 body 和 bindings 在 body 执行期间被回收 ===
    gc_add_root(&rest);         // rest 链包含 bindings 和 body，保护整个尾部
    // 如果 bindings 和 body 是分开的，也可以分别保护
    // gc_add_root(&bindings);
    // gc_add_root(&body);

    int var_count = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b)) var_count++;
    if (var_count == 0) {
        gc_remove_root(&rest);
        return disp_eval_body(body);
    }

    char **var_names = gc_malloc(var_count * sizeof(char*));
    disp_val **expr_vals = gc_malloc(var_count * sizeof(disp_val*));
    // 保护动态数组
    gc_add_root(&var_names);
    gc_add_root(&expr_vals);

    int i = 0;
    disp_val *b = bindings;
    while (b && T(b) == DISP_CONS) {
        disp_val *pair = disp_car(b);
        if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
            gc_remove_root(&expr_vals);
            gc_remove_root(&var_names);
            gc_remove_root(&rest);
            gc_free(var_names); gc_free(expr_vals);
            ERET(NIL, "let: malformed binding (var expr)");
        }
        disp_val *var_sym = disp_car(pair);
        var_names[i] = gc_strdup(disp_get_symbol_name(var_sym));
        expr_vals[i] = disp_car(disp_cdr(pair));
        i++;
        b = disp_cdr(b);
    }

    disp_val **old_vals = gc_malloc(var_count * sizeof(disp_val*));
    gc_add_root(&old_vals);
    for (int j = 0; j < var_count; j++) {
        disp_val *old_sym = disp_find_symbol(var_names[j]);
        old_vals[j] = old_sym ? disp_get_symbol_value(old_sym) : NULL;
        if (old_vals[j] != NULL) gc_add_root(&old_vals[j]);   // 旧值本身保护
    }

    // 求值并绑定
    if (disp_car(expr) == LETA) {   // let*
        for (int j = 0; j < var_count; j++) {
            disp_val *val = disp_eval(expr_vals[j]);
            disp_define_symbol(var_names[j], val, 0);
        }
    } else {
        disp_val **values = gc_malloc(var_count * sizeof(disp_val*));
        gc_add_root(&values);
        for (int j = 0; j < var_count; j++) {
            values[j] = disp_eval(expr_vals[j]);
            gc_add_root(&values[j]);
        }
        for (int j = 0; j < var_count; j++) {
            disp_define_symbol(var_names[j], values[j], 0);
        }
        for (int j = 0; j < var_count; j++) gc_remove_root(&values[j]);
        gc_remove_root(&values);
        gc_free(values);
    }

    // 执行 body（异常安全版本）
    disp_val *result = NIL;
    volatile int normal_exit = 0;
    TRY {
        result = disp_eval_body(body);
        normal_exit = 1;
    } CATCH {
        // 异常路径：恢复绑定，释放资源，然后继续传播
        for (int j = 0; j < var_count; j++) {
            disp_val *restore = old_vals[j];
            disp_define_symbol(var_names[j], restore ? restore : NIL, 0);
            if (restore != NULL) gc_remove_root(&old_vals[j]);
            gc_free(var_names[j]);
        }
        gc_remove_root(&old_vals);
        gc_remove_root(&expr_vals);
        gc_remove_root(&var_names);
        gc_remove_root(&rest);
        gc_free(var_names);
        gc_free(expr_vals);
        gc_free(old_vals);
        THROW(THROWN);
    }
    END_TRY;

    // 正常路径
    if (normal_exit) {
        for (int j = 0; j < var_count; j++) {
            disp_val *restore = old_vals[j];
            disp_define_symbol(var_names[j], restore ? restore : NIL, 0);
            if (restore != NULL) gc_remove_root(&old_vals[j]);
            gc_free(var_names[j]);
        }
        gc_remove_root(&old_vals);
        gc_remove_root(&expr_vals);
        gc_remove_root(&var_names);
        gc_remove_root(&rest);
        gc_free(var_names);
        gc_free(expr_vals);
        gc_free(old_vals);
        return result;
    }
    return NIL;
}

static disp_val* letrec_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "letrec: missing binding list");

    disp_val *first = disp_car(rest);
    if (T(first) == DISP_SYMBOL) {
        disp_val *second_rest = disp_cdr(rest);
        if (second_rest && T(second_rest) == DISP_CONS)
            return letf(expr);
    }

    disp_val *bindings = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body) ERET(NIL, "letrec: missing body");

    int var_count = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b)) var_count++;
    if (var_count == 0) return disp_eval_body(body);

    char **var_names = gc_malloc(var_count * sizeof(char*));
    disp_val **expr_vals = gc_malloc(var_count * sizeof(disp_val*));
    int i = 0;
    disp_val *b = bindings;
    while (b && T(b) == DISP_CONS) {
        disp_val *pair = disp_car(b);
        if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
            gc_free(var_names); gc_free(expr_vals);
            ERET(NIL, "letrec: malformed binding (var expr)");
        }
        disp_val *var_sym = disp_car(pair);
        var_names[i] = gc_strdup(disp_get_symbol_name(var_sym));
        expr_vals[i] = disp_car(disp_cdr(pair));
        i++;
        b = disp_cdr(b);
    }

    // 保存旧值
    disp_val **old_vals = gc_malloc(var_count * sizeof(disp_val*));
    for (int j = 0; j < var_count; j++) {
        disp_val *old_sym = disp_find_symbol(var_names[j]);
        old_vals[j] = old_sym ? disp_get_symbol_value(old_sym) : NULL;
        if (old_vals[j] != NULL) gc_add_root(&old_vals[j]);
    }

    // 占位赋 NIL
    for (int j = 0; j < var_count; j++)
        disp_define_symbol(var_names[j], NIL, 0);

    // 求值并赋值
    if (disp_car(expr) == LETRECA) {   // letrec*
        for (int j = 0; j < var_count; j++) {
            disp_val *val = disp_eval(expr_vals[j]);
            disp_define_symbol(var_names[j], val, 0);
        }
    } else {
        disp_val **values = gc_malloc(var_count * sizeof(disp_val*));
        for (int j = 0; j < var_count; j++) {
            values[j] = disp_eval(expr_vals[j]);
            gc_add_root(&values[j]);
        }
        for (int j = 0; j < var_count; j++) {
            disp_define_symbol(var_names[j], values[j], 0);
        }
        for (int j = 0; j < var_count; j++) gc_remove_root(&values[j]);
        gc_free(values);
    }

    // 执行 body（异常安全）
    disp_val *result = NIL;
    volatile int normal_exit = 0;
    TRY {
        result = disp_eval_body(body);
        normal_exit = 1;
    } CATCH {
        for (int j = 0; j < var_count; j++) {
            disp_val *restore = old_vals[j];
            disp_define_symbol(var_names[j], restore ? restore : NIL, 0);
            if (restore != NULL) gc_remove_root(&old_vals[j]);
            gc_free(var_names[j]);
        }
        gc_free(var_names);
        gc_free(expr_vals);
        gc_free(old_vals);
        THROW(THROWN);
    }
    END_TRY;

    if (normal_exit) {
        for (int j = 0; j < var_count; j++) {
            disp_val *restore = old_vals[j];
            disp_define_symbol(var_names[j], restore ? restore : NIL, 0);
            if (restore != NULL) gc_remove_root(&old_vals[j]);
            gc_free(var_names[j]);
        }
        gc_free(var_names);
        gc_free(expr_vals);
        gc_free(old_vals);
        return result;
    }
    return NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("let"     , MKB(let_builtin    , "<#let>"    ), 1);
    DEF("let*"    , MKB(let_builtin    , "<#let*>"   ), 1);
    DEF("letrec"  , MKB(letrec_builtin , "<#letrec>" ), 1);
    DEF("letrec*" , MKB(letrec_builtin , "<#letrec*>"), 1);
}
