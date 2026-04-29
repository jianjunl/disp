
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

// --- define ---
static disp_val* define_builtin(disp_val *expr) {
    disp_val *cadr = disp_cdr(expr);
    if (!cadr || T(cadr) != DISP_CONS) 
        ERET(NIL, "define: missing first argument");
    
    disp_val *first_arg = disp_car(cadr);
    
    if (T(first_arg) == DISP_SYMBOL) {
        // 可能是 (define symbol expr) 或 (define symbol (params) body ...)
        disp_val *rest = disp_cdr(cadr);
        
        // 检查是否为 (define symbol (params) body ...)
        if (rest && T(rest) == DISP_CONS) {
            disp_val *second = disp_car(rest);          // (params) 或 NIL
            disp_val *body_rest = disp_cdr(rest);       // 剩余表达式列表
            
            // 存在 body 且第二个参数是合法参数列表（NIL 或 CONS）
            if (body_rest && T(body_rest) == DISP_CONS) {
                // 构造 (lambda (params) body1 body2 ...)
                disp_val *params = second;              // 允许 NIL
                disp_val *lambda_expr = disp_make_cons(LAMBDA, 
                                        disp_make_cons(params, body_rest));
                disp_val *closure = disp_eval(lambda_expr);
                disp_define_symbol(disp_get_symbol_name(first_arg), closure, 0);
                return first_arg;
            }
        }
        
        // 普通 (define symbol expr)
        if (!rest || T(rest) != DISP_CONS) 
            ERET(NIL, "define: missing expression");
        disp_val *value = disp_eval(disp_car(rest));
        disp_define_symbol(disp_get_symbol_name(first_arg), value, 0);
        return first_arg;
        
    } else if (T(first_arg) == DISP_CONS) {
        // 原有 (define (name params) body ...) 语法
        disp_val *name_sym = disp_car(first_arg);
        if (T(name_sym) != DISP_SYMBOL) 
            ERET(NIL, "define: function name must be a symbol");
        disp_val *params = disp_cdr(first_arg);
        disp_val *rest = disp_cdr(cadr);
        if (!rest) ERET(NIL, "define: missing body");
        disp_val *lambda_expr = disp_make_cons(LAMBDA, disp_make_cons(params, rest));
        disp_val *closure = disp_eval(lambda_expr);
        disp_define_symbol(disp_get_symbol_name(name_sym), closure, 0);
        return name_sym;
        
    } else {
        ERET(NIL, "define: first argument must be symbol or list\n");
    }
}

// --- set! ---
static disp_val* setq_builtin(disp_val *expr) {
    // (set! symbol expr)
    disp_val *cadr = disp_cdr(expr);
    if (!cadr || T(cadr) != DISP_CONS) ERET(NIL, "set!: missing symbol");
    disp_val *sym = disp_car(cadr);
    if (T(sym) != DISP_SYMBOL) ERET(NIL, "set!: first argument must be a symbol");
    disp_val *rest = disp_cdr(cadr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "set!: missing expression");
    disp_val *value = disp_eval(disp_car(rest));
    disp_define_symbol(disp_get_symbol_name(sym), value, 0); // define updates the symbol's value
    return value;
}

// --- if ---
static disp_val* if_builtin(disp_val *expr) {
    // (if cond then [else])
    disp_val *cadr = disp_cdr(expr);
    if (!cadr || T(cadr) != DISP_CONS) ERET(NIL, "if: missing condition");
    disp_val *cond = disp_eval(disp_car(cadr));
    disp_val *then_rest = disp_cdr(cadr);
    if (!then_rest || T(then_rest) != DISP_CONS) ERET(NIL, "if: missing then clause");
    if (cond != NIL) {
        return disp_eval(disp_car(then_rest));
    } else {
        disp_val *else_rest = disp_cdr(then_rest);
        if (else_rest && T(else_rest) == DISP_CONS)
            return disp_eval(disp_car(else_rest));
        else
            return NIL;
    }
}

// --- cond ---
static disp_val* cond_builtin(disp_val *expr) {
    // (cond (test expr) ...)
    disp_val *clauses = disp_cdr(expr);
    while (clauses && T(clauses) == DISP_CONS) {
        disp_val *clause = disp_car(clauses);
        if (T(clause) != DISP_CONS) {
            ERET(NIL, "cond: malformed clause");
        }
        disp_val *test = disp_eval(disp_car(clause));
        if (test != NIL) {
            // evaluate the rest of the clause (one or more expressions)
            disp_val *body = disp_cdr(clause);
            if (!body || T(body) != DISP_CONS) return NIL;
            // evaluate all but return the last
            disp_val *last = NIL;
            while (body && T(body) == DISP_CONS) {
                last = disp_eval(disp_car(body));
                body = disp_cdr(body);
            }
            return last;
        }
        clauses = disp_cdr(clauses);
    }
    return NIL;
}

// --- while ---
static disp_val* while_builtin(disp_val *expr) {
    // (while test body...)
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "while: missing test");
    disp_val *test_form = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body) return NIL;
    disp_val *last = NIL;
    while (1) {
        disp_val *cond = disp_eval(test_form);
        if (cond == NIL) break;
        // evaluate body forms
        disp_val *body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            last = disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    return last;
}

// --- and ---
static disp_val* and_builtin(disp_val *expr) {
    disp_val *args = disp_cdr(expr);
    disp_val *last = TRUE;
    while (args && T(args) == DISP_CONS) {
        disp_val *val = disp_eval(disp_car(args));
        if (val == NIL) return NIL;
        last = val;
        args = disp_cdr(args);
    }
    return last;
}

// --- or ---
static disp_val* or_builtin(disp_val *expr) {
    disp_val *args = disp_cdr(expr);
    while (args && T(args) == DISP_CONS) {
        disp_val *val = disp_eval(disp_car(args));
        if (val != NIL) return val;
            args = disp_cdr(args);
        }
    return NIL;
}

// --- begin ---
static disp_val* begin_builtin(disp_val *expr) {
    // (begin expr1 expr2 ...)
    disp_val *body = disp_cdr(expr);
    if (!body) return NIL;           // 空 begin 返回 nil
    // 顺序求值所有表达式，返回最后一个的值
    disp_val *last = NIL;
    while (body && T(body) == DISP_CONS) {
        last = disp_eval(disp_car(body));
        body = disp_cdr(body);
    }
    return last;
}

static disp_val* repeat_builtin(disp_val *expr) {
    // 语法: (repeat count body ...)
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) 
        ERET(NIL, "repeat: missing count");
    disp_val *count_val = disp_eval(disp_car(rest));
    long n = (T(count_val) == DISP_INT) ? disp_get_int(count_val) 
            : (T(count_val) == DISP_LONG) ? disp_get_long(count_val) 
            : 0;
    if (n <= 0) return NIL;
    disp_val *body = disp_cdr(rest);
    if (!body) return NIL;
    disp_val *last = NIL;
    for (long i = 0; i < n; i++) {
        disp_val *body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            last = disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    return last ? last : NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("define"  , MKB(define_builtin , "<#define>" ), 1);
    DEF("set!"    , MKB(setq_builtin   , "<#set!>"   ), 1);
    DEF("if"      , MKB(if_builtin     , "<#if>"     ), 1);
    DEF("cond"    , MKB(cond_builtin   , "<#cond>"   ), 1);
    DEF("while"   , MKB(while_builtin  , "<#while>"  ), 1);
    DEF("and"     , MKB(and_builtin    , "<#and>"    ), 1);
    DEF("or"      , MKB(or_builtin     , "<#or>"     ), 1);
    DEF("begin"   , MKB(begin_builtin  , "<#begin>"  ), 1);
    DEF("progn"   , MKB(begin_builtin  , "<#progn>"  ), 1);
    DEF("repeat"  , MKB(repeat_builtin , "<#repeat>" ), 1);
}
