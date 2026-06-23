
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

static disp_val setq_builtin(disp_env_t *env, disp_val expr) {
    disp_val cadr = disp_cdr(expr);
    if (N(cadr) || T(cadr) != FLAG_CONS) ERET(NIL, "set!: missing first argument");
    disp_val sym_or_path = disp_car(cadr);

    // --- 处理 (type ...) 路径 ---
    if (T(sym_or_path) == FLAG_CONS) {
        disp_val head = disp_car(sym_or_path);
        if (T(head) == FLAG_SYMBOL && SYM_ID(head).id == SYM_ID(TYPE).id) {
            disp_val path = disp_cdr(sym_or_path);
            if (N(path) || T(path) != FLAG_CONS)
                ERET(NIL, "set!: (type ...) needs at least two symbols");
            int count = 0;
            disp_val syms[64];
            disp_val p = path;
            while (NE(p, NIL) && T(p) == FLAG_CONS) {
                disp_val sym = disp_car(p);
                if (T(sym) != FLAG_SYMBOL)
                    ERET(NIL, "set!: path elements must be symbols");
                syms[count++] = sym;
                p = disp_cdr(p);
            }
            if (NE(p, NIL))
                ERET(NIL, "set!: malformed (type ...) path");
            if (count < 2)
                ERET(NIL, "set!: (type ...) needs at least two symbols (type and field)");

            // 提取最后一个符号作为要设置的字段名
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
                ERET(NIL, "set!: prefix expression must evaluate to a type");

            disp_env_t *tenv = disp_get_type_env(pt);
            if (!tenv) ERET(NIL, "set!: type has no environment");

            // 计算新值
            disp_val rest = disp_cdr(cadr);
            if (N(rest) || T(rest) != FLAG_CONS)
                ERET(NIL, "set!: missing expression");
            disp_val new_value = disp_eval(env, disp_car(rest));

            // 查找字段是否存在
            disp_val found = disp_find_symbol(tenv, field_id);
            if (N(found))
                ERET(NIL, "set!: field '%s' not defined in type", disp_get_name(field_id.id));
            disp_set_symbol_value_unlock(tenv, found, new_value);
            return new_value;
        }
    }

    // --- 原有的符号处理 ---
    if (T(sym_or_path) != FLAG_SYMBOL)
        ERET(NIL, "set!: first argument must be a symbol or (type ...)");
    disp_sid id = SYM_ID(sym_or_path);
    disp_val rest = disp_cdr(cadr);
    if (N(rest) || T(rest) != FLAG_CONS) ERET(NIL, "set!: missing expression");
    disp_val new_value = disp_eval(env, disp_car(rest));
    disp_val found_sym = disp_find_symbol(env, id);
    if (N(found_sym)) {
        ERET(NIL, "set!: undefined variable '%s'", disp_get_name(id.id));
    }
    disp_set_symbol_value_unlock(env, found_sym, new_value);
    return new_value;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    REG("set!"    , MKB(setq_builtin   , "<#set!>"   ), 1);
    REG("setq"    , MKB(setq_builtin   , "<#set!>"   ), 1);
}
