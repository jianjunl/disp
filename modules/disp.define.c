
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

    // --- 处理 (type ...) 路径 ---
    if (T(first_arg) == FLAG_CONS) {
        disp_val head = disp_car(first_arg);
        if (T(head) == FLAG_SYMBOL && SYM_ID(head).id == SYM_ID(TYPE).id) {
            disp_val path = disp_cdr(first_arg);
            if (N(path) || T(path) != FLAG_CONS)
                ERET(NIL, "define: (type ...) needs at least two symbols");
            int count = 0;
            disp_val syms[64];
            disp_val p = path;
            while (NE(p, NIL) && T(p) == FLAG_CONS) {
                disp_val sym = disp_car(p);
                if (T(sym) != FLAG_SYMBOL)
                    ERET(NIL, "define: path elements must be symbols");
                syms[count++] = sym;
                p = disp_cdr(p);
            }
            if (NE(p, NIL))
                ERET(NIL, "define: malformed (type ...) path");
            if (count < 2)
                ERET(NIL, "define: (type ...) needs at least two symbols (type and field)");

            // 提取最后一个符号作为要定义的字段名
            disp_sid field_id = SYM_ID(syms[count-1]);

            // 构造前缀列表 (type t1 ... tn-1)
            disp_val prefix = NIL;
            disp_val tail = NIL;
            for (int i = 0; i < count - 1; i++) {
                disp_val new_cons = disp_make_cons(syms[i], NIL);
                if (E(prefix, NIL)) {
                    prefix = new_cons;
                    tail = new_cons;
                } else {
                    disp_set_cdr(tail, new_cons);
                    tail = new_cons;
                }
            }
            disp_val prefix_expr = disp_make_cons(TYPE, prefix);

            // 求值前缀得到类型对象
            disp_val pt = disp_eval(env, prefix_expr);
            if (N(pt) || T(pt) != FLAG_TYPE)
                ERET(NIL, "define: prefix expression must evaluate to a type");

            disp_env_t *tenv = disp_get_type_env(pt);
            if (!tenv) ERET(NIL, "define: type has no environment");

            // 计算要赋的值
            disp_val rest = disp_cdr(cadr);
            if (N(rest) || T(rest) != FLAG_CONS)
                ERET(NIL, "define: missing expression for (type ...)");
            disp_val value = disp_eval(env, disp_car(rest));

            disp_define_symbol(tenv, field_id, value, 0);
            return syms[count-1];  // 返回字段符号
        }
    }
    
    if (T(first_arg) == FLAG_SYMBOL) {
        disp_val rest = disp_cdr(cadr);
        if (N(rest) || T(rest) != FLAG_CONS) 
            ERET(NIL, "define: missing expression");
        
        disp_val expr_form = disp_car(rest);   // 表达式部分
        // 注意：我们删除了函数简写形式 (define symbol (params) body...)
        // 但保留 (define symbol (lambda params body...)) 以支持尾递归优化
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
        
        // 普通 (define symbol expr)
        disp_val value = disp_eval(env, expr_form);
        disp_define_symbol(env, SYM_ID(first_arg), value, 0);
        return first_arg;
    } 
    else if (T(first_arg) == FLAG_CONS) {
        // 保留 (define (name params) body ...) 语法
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

// --- defun ---
static disp_val defun_builtin(disp_env_t *env, disp_val expr) {
    disp_val cadr = disp_cdr(expr);
    if (N(cadr) || T(cadr) != FLAG_CONS)
        ERET(NIL, "defun: missing function name or signature");
    
    disp_val name_or_sig = disp_car(cadr);
    disp_val rest = disp_cdr(cadr);

    // 拒绝 (type ...) 形式
    if (T(name_or_sig) == FLAG_CONS) {
        disp_val head = disp_car(name_or_sig);
        if (T(head) == FLAG_SYMBOL && SYM_ID(head).id == SYM_ID(TYPE).id) {
            ERET(NIL, "defun: cannot use (type ...) as function name");
        }
    }
    
    if (T(name_or_sig) == FLAG_SYMBOL) {
        // (defun symbol (params) body1 body2 ...)
        if (N(rest) || T(rest) != FLAG_CONS)
            ERET(NIL, "defun: missing parameter list");
        disp_val params = disp_car(rest);
        disp_val body_rest = disp_cdr(rest);
        if (N(body_rest))
            ERET(NIL, "defun: missing body");
        // 参数列表应为 cons 或 nil（允许无参）
        if (T(params) != FLAG_CONS && NE(params, NIL))
            ERET(NIL, "defun: parameter list must be a list");
        // 创建闭包，body_rest 隐含 begin
        disp_val closure = disp_make_closure(env, params, body_rest, 1);
        disp_define_symbol(env, SYM_ID(name_or_sig), closure, 0);
        return name_or_sig;
    }
    else if (T(name_or_sig) == FLAG_CONS) {
        // (defun (name params) body1 body2 ...)
        disp_val name_sym = disp_car(name_or_sig);
        if (T(name_sym) != FLAG_SYMBOL)
            ERET(NIL, "defun: function name must be a symbol");
        disp_val params = disp_cdr(name_or_sig);
        // 参数列表应为 cons 或 nil
        if (T(params) != FLAG_CONS && NN(params))
            ERET(NIL, "defun: parameter list must be a list");
        if (N(rest))
            ERET(NIL, "defun: missing body");
        // rest 即为 body 列表
        disp_val closure = disp_make_closure(env, params, rest, 1);
        disp_define_symbol(env, SYM_ID(name_sym), closure, 0);
        return name_sym;
    }
    else {
        ERET(NIL, "defun: first argument must be symbol or list");
    }
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    REG("define"  , MKB(define_builtin , "<#define>" ), 1);
    REG("defun"   , MKB(defun_builtin  , "<#defun>"  ), 1);
}
