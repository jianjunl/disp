
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
static disp_box define_builtin(disp_scope_t *scope, disp_box expr) {
    disp_box cadr = disp_cdr(expr);
    if (!cadr || T(cadr) != FLAG_CONS) 
        ERET(NIL, "define: missing first argument");
    
    disp_box first_arg = disp_car(cadr);
    
    if (T(first_arg) == FLAG_SYMBOL) {
        // 可能是 (define symbol expr) 或 (define symbol (params) body ...)
        disp_box rest = disp_cdr(cadr);
        
        // 检查是否为 (define symbol (params) body ...)
        if (rest && T(rest) == FLAG_CONS) {
            disp_box second = disp_car(rest);          // (params) 或 NIL
            disp_box body_rest = disp_cdr(rest);       // 剩余表达式列表
            
            // 存在 body 且第二个参数是合法参数列表（NIL 或 CONS）
            if (body_rest && T(body_rest) == FLAG_CONS) {
                // 构造 (lambda (params) body1 body2 ...)
                disp_box params = second;              // 允许 NIL
                // 原来构造 lambda_expr 再求值，改为直接创建闭包
                //disp_box lambda_expr = disp_make_cons(LAMBDA, disp_make_cons(params, body_rest));
                //disp_box closure = disp_eval(scope, lambda_expr);
                //disp_define_symbol(scope, S(first_arg), closure, 0);
                // 直接创建可尾递归优化的闭包
                disp_box closure = disp_make_closure(scope, params, body_rest, 1);
                disp_define_symbol(scope, S(first_arg), closure, 0);
                return first_arg;
            }
        }
        
        // 普通 (define symbol expr)
        if (!rest || T(rest) != FLAG_CONS) 
            ERET(NIL, "define: missing expression");
        disp_box value = disp_eval(scope, disp_car(rest));
        disp_define_symbol(scope, S(first_arg), value, 0);
        return first_arg;
    } else if (T(first_arg) == FLAG_CONS) {
        // 原有 (define (name params) body ...) 语法
        disp_box name_sym = disp_car(first_arg);
        if (T(name_sym) != FLAG_SYMBOL) 
            ERET(NIL, "define: function name must be a symbol");
        disp_box params = disp_cdr(first_arg);
        disp_box rest = disp_cdr(cadr);
        if (!rest) ERET(NIL, "define: missing body");
        // 原来构造 lambda_expr 再求值，改为直接创建闭包
        //disp_box lambda_expr = disp_make_cons(LAMBDA, disp_make_cons(params, rest));
        //disp_box closure = disp_eval(scope, lambda_expr);
        //disp_define_symbol(scope, S(name_sym), closure, 0);
        // 直接创建可尾递归优化的闭包
        disp_box closure = disp_make_closure(scope, params, rest, 1);
        disp_define_symbol(scope, S(name_sym), closure, 0);
        return name_sym;
        
    } else {
        ERET(NIL, "define: first argument must be symbol or list\n");
    }
}

static disp_box setq_builtin(disp_scope_t *scope, disp_box expr) {
    disp_box cadr = disp_cdr(expr);
    if (!cadr || T(cadr) != FLAG_CONS) ERET(NIL, "set!: missing symbol");
    disp_box sym = disp_car(cadr);
    if (T(sym) != FLAG_SYMBOL) ERET(NIL, "set!: first argument must be a symbol");
    const char *name = disp_get_symbol_name(sym);
    
    disp_box rest = disp_cdr(cadr);
    if (!rest || T(rest) != FLAG_CONS) ERET(NIL, "set!: missing expression");
    disp_box new_value = disp_eval(scope, disp_car(rest));
    
    disp_box found_sym = disp_find_symbol(scope, name);
    if (!found_sym) {
        ERET(NIL, "set!: undefined variable '%s'", name);
    }
    disp_set_symbol_value(found_sym, new_value);
    return new_value;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("define"  , MKB(define_builtin , "<#define>" ), 1);
    DEF("set!"    , MKB(setq_builtin   , "<#set!>"   ), 1);
    DEF("setq"    , MKB(setq_builtin   , "<#set!>"   ), 1);
}
