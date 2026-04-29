
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

// --- lambda ---
static disp_val* lambda_builtin(disp_val *expr) {
    // (lambda (params) body...)
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "lambda: missing parameter list");
    disp_val *params = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body) ERET(NIL, "lambda: missing body");
    return disp_make_closure(params, body);
}

// --- macro ---
static disp_val* macro_builtin(disp_val *expr) {
    // (macro (params) body...)
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "macro: missing parameter list");
    disp_val *params = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body) ERET(NIL, "macro: missing body");
    return disp_make_macro(params, body);
}

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

// --- let/let* ---
static disp_val* let_builtin(disp_val *expr) {
    // 语法: (let ((var1 expr1) ...) body ...)  或 (let name ((var init) ...) body ...)
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "let: missing binding list");
    
    // 检查是否为命名 let：rest 的第一个元素是符号，且 cdr 存在且为 CONS
    disp_val *first = disp_car(rest);
    if (T(first) == DISP_SYMBOL) {
        disp_val *second_rest = disp_cdr(rest);
        if (second_rest && T(second_rest) == DISP_CONS) {
            // 可能是命名 let，调用 letf 处理
            return letf(expr);
        }
    }
    
    // 原 let 逻辑保持不变...
    disp_val *bindings = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body) ERET(NIL, "let: missing body");

    // 1. 收集变量名和表达式（未求值）
    int var_count = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b)) var_count++;
    if (var_count == 0) {
        // 没有绑定，直接执行 body
        return disp_eval_body(body);
    }

    char **var_names = gc_malloc(var_count * sizeof(char*));
    disp_val **expr_vals = gc_malloc(var_count * sizeof(disp_val*));
    int i = 0;
    disp_val *b = bindings;
    while (b && T(b) == DISP_CONS) {
        disp_val *pair = disp_car(b);
        if (T(pair) != DISP_CONS || T(disp_car(pair)) != DISP_SYMBOL) {
            gc_free(var_names); gc_free(expr_vals);
            ERET(NIL, "let: malformed binding (var expr)");
        }
        disp_val *var_sym = disp_car(pair);
        var_names[i] = gc_strdup(disp_get_symbol_name(var_sym));
        expr_vals[i] = disp_car(disp_cdr(pair));   // 暂存表达式
        i++;
        b = disp_cdr(b);
    }

    // 2. 保存旧值（如果需要恢复，但 let 是局部作用域，我们用占位 + 恢复的方式）
    // 先保存旧值
    disp_val **old_vals = gc_malloc(var_count * sizeof(disp_val*));
    for (int j = 0; j < var_count; j++) {
        disp_val *old_sym = disp_find_symbol(var_names[j]);
        old_vals[j] = old_sym ? disp_get_symbol_value(old_sym) : NULL;
    }

    // 3. 求值并绑定（let 同时求值再赋值，let* 顺序求值并立即赋值）
// 原代码
//if (disp_car(expr) == LETA) {
// 改为
//disp_val *car_expr = disp_car(expr);
//if (car_expr != NIL && T(car_expr) == DISP_SYMBOL && strcmp(disp_get_symbol_name(car_expr), "let*") == 0) {
    if (disp_car(expr) == LETA) {
        // let*: 顺序求值，每个 expr 可以引用之前绑定的变量
        for (int j = 0; j < var_count; j++) {
            disp_val *val = disp_eval(expr_vals[j]);
            disp_define_symbol(var_names[j], val, 0);
        }
    } else { // let
        // let: 先全部求值，再统一赋值（expr 中不能引用同批次的变量）
        disp_val **values = gc_malloc(var_count * sizeof(disp_val*));
        for (int j = 0; j < var_count; j++) {
            values[j] = disp_eval(expr_vals[j]);
        }
        for (int j = 0; j < var_count; j++) {
            disp_define_symbol(var_names[j], values[j], 0);
        }
        gc_free(values);
    }

    // 4. 执行 body
    disp_val *result = disp_eval_body(body);

    // 5. 恢复旧值
    for (int j = 0; j < var_count; j++) {
        if (old_vals[j] != NULL)
            disp_define_symbol(var_names[j], old_vals[j], 0);
        else
            disp_define_symbol(var_names[j], NIL, 0);
        gc_free(var_names[j]);
    }
    gc_free(var_names);
    gc_free(expr_vals);
    gc_free(old_vals);

    return result;
}

// --- letrec/letrec* ---
static disp_val* letrec_builtin(disp_val *expr) {
    // 语法: (letrec ((var1 expr1) ...) body ...) 或命名形式 (letrec name ...)
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "letrec: missing binding list");
    
    disp_val *first = disp_car(rest);
    if (T(first) == DISP_SYMBOL) {
        disp_val *second_rest = disp_cdr(rest);
        if (second_rest && T(second_rest) == DISP_CONS) {
            // 命名 letrec 也可用相同机制，复用 letf（将 let 改为 letrec 即可）
            // 或者单独实现 letrecf，但这里为了简单，调用 letf 并将生成的 letrec 改为 letrec
            // 注意：letf 内部使用 LETRECA，所以命名 letrec 也可以直接使用 letf
            return letf(expr);
        }
    }
    
    // 原 letrec 逻辑...
    disp_val *bindings = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body) ERET(NIL, "letrec: missing body");

    // 1. 收集变量名和表达式
    int var_count = 0;
    for (disp_val *b = bindings; b && T(b) == DISP_CONS; b = disp_cdr(b)) var_count++;
    if (var_count == 0) {
        // 没有绑定，直接执行 body
        return disp_eval_body(body);
    }

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
        expr_vals[i] = disp_car(disp_cdr(pair));   // 暂存表达式（未求值）
        i++;
        b = disp_cdr(b);
    }

    // 2. 创建所有变量并赋初始值为 NIL（占位）
    for (int j = 0; j < var_count; j++) {
        disp_define_symbol(var_names[j], NIL, 0);
    }

    // 3. 如果是 letrec*，顺序求值并赋值；如果是 letrec，先全部求值再赋值
// 原代码
//if (disp_car(expr) == LETRECA) {
// 改为
//disp_val *car_expr = disp_car(expr);
//if (car_expr != NIL && T(car_expr) == DISP_SYMBOL && strcmp(disp_get_symbol_name(car_expr), "letrec*") == 0) {
    if (disp_car(expr) == LETRECA) {
        // 顺序求值并立即赋值
        for (int j = 0; j < var_count; j++) {
            disp_val *val = disp_eval(expr_vals[j]);
            disp_define_symbol(var_names[j], val, 0);
        }
    } else { // letrec
        // 先全部求值，保存结果
        disp_val **values = gc_malloc(var_count * sizeof(disp_val*));
        for (int j = 0; j < var_count; j++) {
            values[j] = disp_eval(expr_vals[j]);
        }
        // 再赋值
        for (int j = 0; j < var_count; j++) {
            disp_define_symbol(var_names[j], values[j], 0);
        }
        gc_free(values);
    }

    // 4. 执行 body
    disp_val *result = disp_eval_body(body);

    // 5. 清理临时变量（可选：恢复旧值或直接设为 NIL）
    for (int j = 0; j < var_count; j++) {
        disp_define_symbol(var_names[j], NIL, 0);
        gc_free(var_names[j]);
    }
    gc_free(var_names);
    gc_free(expr_vals);

    return result;
}

/* ----- apply ----- */
static disp_val* apply_syscall(disp_val **args, int count) {
    if (count != 2) {
        ERET(NIL, "apply: expects two arguments (func arg-list)");
    }
    disp_val *func = args[0];
    disp_val *arg_list = args[1];

    // 检查参数列表是否为 proper list（末尾必须是 nil）
    disp_val *p = arg_list;
    while (p != NIL && T(p) == DISP_CONS) {
        p = disp_cdr(p);
    }
    if (p != NIL) {
        ERET(NIL, "apply: argument list must be a proper list");
    }

    // 计算列表长度
    int arg_count = 0;
    for (disp_val *a = arg_list; a != NIL; a = disp_cdr(a)) {
        arg_count++;
    }

    // 分配参数数组
    disp_val **argv = gc_malloc(arg_count * sizeof(disp_val*));
    if (!argv) return NIL;
    int i = 0;
    for (disp_val *a = arg_list; a != NIL; a = disp_cdr(a)) {
        argv[i++] = disp_car(a);
    }

    disp_val *result = NIL;
    if (T(func) == DISP_SYSCALL) {
        result = disp_get_syscall(func)(argv, arg_count);
    } else if (T(func) == DISP_CLOSURE) {
        result = disp_apply_closure(func, argv, arg_count);
    } else if (T(func) == DISP_BUILTIN) {
        // 内置特殊形式通常不能通过 apply 直接调用，但我们可以尝试将列表构造成表达式再求值？
        // 简单处理：报错
        gc_free(argv);
        ERET(NIL, "apply: cannot apply built-in special form");
    } else {
        gc_free(argv);
        ERET(NIL, "apply: first argument must be a function or closure");
    }

    gc_free(argv);
    return result;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("lambda"  , MKB(lambda_builtin , "<#lambda>" ), 1);
    DEF("macro"   , MKB(macro_builtin  , "<#macro>"  ), 1);
    DEF("let"     , MKB(let_builtin    , "<#let>"    ), 1);
    DEF("let*"    , MKB(let_builtin    , "<#let*>"   ), 1);
    DEF("letrec"  , MKB(letrec_builtin , "<#letrec>" ), 1);
    DEF("letrec*" , MKB(letrec_builtin , "<#letrec*>"), 1);
    DEF("apply"   , MKF(apply_syscall  , "<apply>"   ), 1);
}
