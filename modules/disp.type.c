
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

    disp_val v = disp_find_symbol(env, SYM_ID(sym));
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

        v = disp_find_symbol(tenv, SYM_ID(sym));
        if (N(v))
            ERET(NIL, "type: symbol '%s' not found in type", SYM_NAME(sym));

        args = disp_cdr(args);
    }
    if (NN(args))
        ERET(NIL, "type: extra arguments");

    return v;
}

// --- new ---
static disp_val new_builtin(disp_env_t *env, disp_val expr) {
    disp_val args = disp_cdr(expr);
    // 至少需要 super, newtype 和一个 body 表达式
    if (N(args) || T(args) != FLAG_CONS)
        ERET(NIL, "new: missing arguments");
    disp_val super = disp_car(args);
    args = disp_cdr(args);
    if (N(args) || T(args) != FLAG_CONS)
        ERET(NIL, "new: missing newtype");
    disp_val newtype = disp_car(args);
    args = disp_cdr(args);
    if (N(args))   // body 至少有一个表达式
        ERET(NIL, "new: missing body");

    // --- 求值 super ---
    disp_val parent;
    if (E(super, NIL)) {
        // super 为 NIL 时，使用 proto 作为父类型
        parent = GSYM(PROTO);
        if (N(parent) || T(parent) != FLAG_TYPE)
            ERET(NIL, "new: proto not found or not a type");
    } else if (T(super) == FLAG_SYMBOL) {
        disp_val sv = disp_find_symbol(env, SYM_ID(super));
        if (N(sv))
            ERET(NIL, "new: super symbol '%s' not found", SYM_NAME(super));
        if (T(sv) != FLAG_TYPE)
            ERET(NIL, "new: super must be a type");
        parent = sv;
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

    // --- 解析 newtype ---
    // 必须为 (type n1 n2 ... n)
    if (T(newtype) != FLAG_CONS)
        ERET(NIL, "new: newtype must be a list");
    disp_val type_head = disp_car(newtype);
    if (T(type_head) != FLAG_SYMBOL || strcmp(SYM_NAME(type_head), "type") != 0)
        ERET(NIL, "new: newtype must begin with 'type'");
    disp_val type_args = disp_cdr(newtype);  // 符号列表
    if (N(type_args) || T(type_args) != FLAG_CONS)
        ERET(NIL, "new: newtype needs at least one symbol");

    // 提取最后一个符号作为新类型名，前面的构造为 (type n1 ... n_{k-1})
    disp_val *last_ptr = NULL;
    disp_val prev = NIL;
    disp_val cur = type_args;
    while (NN(cur) && T(cur) == FLAG_CONS) {
        prev = cur;
        cur = disp_cdr(cur);
    }
    if (NE(cur, NIL))
        ERET(NIL, "new: malformed newtype list");
    // prev 是最后一个 cons，其 car 是最后一个符号
    disp_val last_sym = disp_car(prev);
    if (T(last_sym) != FLAG_SYMBOL)
        ERET(NIL, "new: last element of newtype must be a symbol");

    // 构造 newt 列表 (type n1 ... n_{k-1})
    disp_val newt = NIL;
    disp_val iter = type_args;
    while (NN(iter) && T(iter) == FLAG_CONS) {
        disp_val sym = disp_car(iter);
        if (T(sym) != FLAG_SYMBOL)
            ERET(NIL, "new: newtype elements must be symbols");
        // 如果不是最后一个，加入新列表
        if (!E(iter, prev)) {
            newt = disp_make_cons(sym, newt); // 反向构建，最后反转
        }
        iter = disp_cdr(iter);
    }
    // 反转 newt
    disp_val reversed = NIL;
    while (NN(newt) && T(newt) == FLAG_CONS) {
        reversed = disp_make_cons(disp_car(newt), reversed);
        newt = disp_cdr(newt);
    }
    newt = reversed;
    // 求值 newt 得到父类型（用于类型查找）
    disp_val parent_for_new;
    if (E(newt, NIL)) {
        // 如果没有前缀，则父类型就是 parent（已经求值）
        parent_for_new = parent;
    } else {
        // 构造 (type n1 ... n_{k-1}) 表达式并求值
        disp_val type_expr = disp_make_cons(TYPE, newt);
        parent_for_new = disp_eval(env, type_expr);
        if (N(parent_for_new))
            ERET(NIL, "new: cannot evaluate newtype prefix");
        if (T(parent_for_new) != FLAG_TYPE)
            ERET(NIL, "new: newtype prefix must evaluate to a type");
    }

    // --- 创建新类型 ---
    // disp_new_type 如果 parent 为 NIL 则返回 proto，我们已确保 parent 为类型，所以安全
    disp_val new_type = disp_new_type(parent_for_new);
    if (N(new_type) || T(new_type) != FLAG_TYPE)
        ERET(NIL, "new: failed to create new type");

    // --- 在父类型环境中绑定新类型名 ---
    disp_env_t *parent_env = disp_get_type_env(parent_for_new);
    if (!parent_env)
        ERET(NIL, "new: parent type has no environment");
    disp_set_symbol_value_unlock(parent_env, last_sym, new_type);

    // --- 创建新的执行环境，绑定 THIS ---
    disp_env_t *new_env = disp_new_env(env);
    if (!new_env) ERET(NIL, "new: out of memory");
    disp_val this_sym = disp_make_symbol(THIS);
    disp_set_symbol_value_unlock(new_env, this_sym, new_type); // 绑定 THIS

    // --- 在 new_env 下顺序执行 body ---
    disp_val body = args;  // 剩余的都是 body
    disp_val last = NIL;
    while (NN(body) && T(body) == FLAG_CONS) {
        last = disp_eval(new_env, disp_car(body));
        body = disp_cdr(body);
    }
    return NN(last) ? last : NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    REG("type"  , MKB(type_builtin , "<#type>" ), 1);
    REG("new"   , MKB(new_builtin  , "<#new>"  ), 1);
}
