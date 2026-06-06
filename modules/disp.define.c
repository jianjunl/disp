#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // 添加 string.h 用于 strcmp
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
        disp_val rest = disp_cdr(cadr);
        if (N(rest) || T(rest) != FLAG_CONS) 
            ERET(NIL, "define: missing expression");
        
        disp_val expr_form = disp_car(rest);   // 表达式部分
        disp_val body_rest = disp_cdr(rest);   // 后续表达式（仅用于函数简写）
        
        // 1. 处理 (define symbol (lambda params body...))
        if (T(expr_form) == FLAG_CONS) {
            disp_val head = disp_car(expr_form);
            if (T(head) == FLAG_SYMBOL) {
                if (SYM_ID(head).id == LAMBDA.id) {
                    // 提取 (lambda params body...)
                    disp_val lambda_body = disp_cdr(expr_form);
                    if (N(lambda_body) || T(lambda_body) != FLAG_CONS)
                        ERET(NIL, "define: malformed lambda");
                    disp_val params = disp_car(lambda_body);
                    disp_val body = disp_cdr(lambda_body);
                    if (N(body))
                        ERET(NIL, "define: lambda missing body");
                    // 创建可尾递归优化的闭包
                    disp_val closure = disp_make_closure(env, params, body, 1);
                    disp_define_symbol(env, SYM_ID(first_arg), closure, 0);
                    return first_arg;
                }
            }
        }
        
        // 2. 函数简写形式: (define symbol (params) body1 body2 ...)
        //    注意：需要确保 body_rest 非空且为列表，并且 expr_form 是参数列表
        if (NN(body_rest) && T(body_rest) == FLAG_CONS) {
            // 此时 expr_form 应为参数列表
            disp_val params = expr_form;
            disp_val closure = disp_make_closure(env, params, body_rest, 1);
            disp_define_symbol(env, SYM_ID(first_arg), closure, 0);
            return first_arg;
        }
        
        // 3. 普通 (define symbol expr)
        disp_val value = disp_eval(env, expr_form);
        disp_define_symbol(env, SYM_ID(first_arg), value, 0);
        return first_arg;
    } 
    else if (T(first_arg) == FLAG_CONS) {
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
    } 
    else {
        ERET(NIL, "define: first argument must be symbol or list\n");
    }
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("define"  , MKB(define_builtin , "<#define>" ), 1);
}
