
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

// --- type ---
static disp_val type_builtin(disp_env_t *env, disp_val expr) {
    disp_val args = disp_cdr(expr);
    if (N(args) || T(args) != FLAG_CONS)
        ERET(NIL, "type: missing arguments");

    disp_val sym = disp_car(args);
    if (T(sym) != FLAG_SYMBOL)
        ERET(NIL, "type: arguments must be symbols");

    disp_val v = disp_eval(env, sym);
    if (N(v))
        ERET(NIL, "type: symbol '%s' not found", SYM_NAME(sym));

    args = disp_cdr(args);
    while (NN(args) && T(args) == FLAG_CONS) {
        if (T(v) != FLAG_TYPE)
            ERET(NIL, "type: cannot continue, current value is not a type");
        disp_env_t *tenv = disp_get_type_env(v);
        if (!tenv)
            ERET(NIL, "type: invalid type environment");

        sym = disp_car(args);
        if (T(sym) != FLAG_SYMBOL)
            ERET(NIL, "type: arguments must be symbols");

        v = disp_eval(tenv, sym);
        if (N(v))
            ERET(NIL, "type: symbol '%s' not found in type", SYM_NAME(sym));

        args = disp_cdr(args);
    }
    if (NE(args, NIL))   // 使用 NE 代替 NN，确保 args 确实是 NIL
        ERET(NIL, "type: extra arguments");

    return v;
}

// --- new ---
static disp_val new_builtin(disp_env_t *env, disp_val expr) {
    disp_val args = disp_cdr(expr);
    if (N(args) || T(args) != FLAG_CONS)
        ERET(NIL, "new: missing arguments");
    disp_val super = disp_car(args);
    args = disp_cdr(args);
    if (N(args))   // body 至少有一个表达式
        ERET(NIL, "new: missing body");

    // --- 求值 super ---
    disp_val parent;
    if (E(super, NIL)) {
        parent = disp_eval(env, GSYM(PROTO));
        if (N(parent) || T(parent) != FLAG_TYPE)
            ERET(NIL, "new: proto not found or not a type");
    } else if (T(super) == FLAG_SYMBOL) {
        disp_val sv = disp_eval(env, super);
        if (N(sv))
            ERET(NIL, "new: super symbol '%s' not found", SYM_NAME(super));
        if (T(sv) != FLAG_TYPE)
            ERET(NIL, "new: super must be a type");
        parent = sv;   // 直接使用找到的值，不再 eval
    } else if (T(super) == FLAG_CONS) {
        disp_val sv = disp_eval(env, super);
        if (N(sv))
            ERET(NIL, "new: super expression evaluated to nil");
        if (T(sv) != FLAG_TYPE)
            ERET(NIL, "new: super expression must evaluate to a type");
        parent = sv;
    } else {
        ERET(NIL, "new: super must be a symbol, a list, or nil");
    }

    // --- 创建新类型 ---
    disp_val new_type = disp_new_type(parent);
    if (N(new_type) || T(new_type) != FLAG_TYPE)
        ERET(NIL, "new: failed to create new type");

    // --- 创建新的执行环境（继承当前环境），绑定 THIS ---
    disp_env_t *new_env = disp_new_env(env);
    if (!new_env) ERET(NIL, "new: out of memory");
    disp_val this_sym = disp_make_symbol(THIS);
    disp_set_symbol_value_unlock(new_env, this_sym, new_type); // 绑定 THIS

    // --- 在 new_env 下顺序执行 body ---
    disp_val body = args;  // 剩余的都是 body
    while (NN(body) && T(body) == FLAG_CONS) {
        disp_eval(new_env, disp_car(body));
        body = disp_cdr(body);
    }
    // 返回新类型（忽略 body 的返回值）
    return new_type;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    REG("type"  , MKB(type_builtin , "<#type>" ), 1);
    REG("new"   , MKB(new_builtin  , "<#new>"  ), 1);
}
