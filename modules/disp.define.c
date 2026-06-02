
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
static disp_val define_builtin(disp_env_t *env, disp_val expr) {
    disp_val cadr = disp_cdr(expr);
    if (N(cadr) || T(cadr) != FLAG_CONS) 
        ERET(NIL, "define: missing first argument");
    
    disp_val first_arg = disp_car(cadr);
    
    if (T(first_arg) == FLAG_SYMBOL) {
        // 可能是 (define symbol expr) 或 (define symbol (params) body ...)
        disp_val rest = disp_cdr(cadr);
        
        // 检查是否为 (define symbol (params) body ...)
        if (NN(rest) && T(rest) == FLAG_CONS) {
            disp_val second = disp_car(rest);          // (params) 或 NIL
            disp_val body_rest = disp_cdr(rest);       // 剩余表达式列表
            
            // 存在 body 且第二个参数是合法参数列表（NIL 或 CONS）
            if (NN(body_rest) && T(body_rest) == FLAG_CONS) {
                // 构造 (lambda (params) body1 body2 ...)
                disp_val params = second;              // 允许 NIL
                // 直接创建可尾递归优化的闭包
                disp_val closure = disp_make_closure(env, params, body_rest, 1);
                disp_define_symbol(env, SYM_ID(first_arg), closure, 0);
                return first_arg;
            }
        }
        
        // 普通 (define symbol expr)
        if (N(rest) || T(rest) != FLAG_CONS) 
            ERET(NIL, "define: missing expression");
        disp_val value = disp_eval(env, disp_car(rest));
        disp_define_symbol(env, SYM_ID(first_arg), value, 0);
        return first_arg;
    } else if (T(first_arg) == FLAG_CONS) {
        // 原有 (define (name params) body ...) 语法
        disp_val name_sym = disp_car(first_arg);
        if (T(name_sym) != FLAG_SYMBOL) 
            ERET(NIL, "define: function name must be a symbol");
        disp_val params = disp_cdr(first_arg);
        disp_val rest = disp_cdr(cadr);
        if (N(rest)) ERET(NIL, "define: missing body");
        // 直接创建可尾递归优化的闭包
        disp_val closure = disp_make_closure(env, params, rest, 1);
        disp_define_symbol(env, SYM_ID(name_sym), closure, 0);
        return name_sym;
        
    } else {
        ERET(NIL, "define: first argument must be symbol or list\n");
    }
}

static disp_val setq_builtin(disp_env_t *env, disp_val expr) {
    disp_val cadr = disp_cdr(expr);
    if (N(cadr) || T(cadr) != FLAG_CONS) ERET(NIL, "set!: missing symbol");
    disp_val sym = disp_car(cadr);
    if (T(sym) != FLAG_SYMBOL) ERET(NIL, "set!: first argument must be a symbol");
    uint64_t id = SYM_ID(sym);
    
    disp_val rest = disp_cdr(cadr);
    if (N(rest) || T(rest) != FLAG_CONS) ERET(NIL, "set!: missing expression");
    disp_val new_value = disp_eval(env, disp_car(rest));
    
    disp_val found_sym = disp_find_symbol(env, id);
    if (N(found_sym)) {
        ERET(NIL, "set!: undefined variable '%s'", disp_get_name(id));
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
