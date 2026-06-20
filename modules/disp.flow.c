
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

// --- if ---
static disp_val if_builtin(disp_env_t *env, disp_val expr) {
    // (if cond then [else])
    disp_val cadr = disp_cdr(expr);
    if (N(cadr) || T(cadr) != FLAG_CONS) ERET(NIL, "if: missing condition");
    disp_val cond = disp_eval(env, disp_car(cadr));
    disp_val then_rest = disp_cdr(cadr);
    if (N(then_rest) || T(then_rest) != FLAG_CONS) ERET(NIL, "if: missing then clause");
    if (NE(cond, NIL)) {
        return disp_eval(env, disp_car(then_rest));
    } else {
        disp_val else_rest = disp_cdr(then_rest);
        if (NN(else_rest) && T(else_rest) == FLAG_CONS)
            return disp_eval(env, disp_car(else_rest));
        else
            return NIL;
    }
}

// --- cond ---
static disp_val cond_builtin(disp_env_t *env, disp_val expr) {
    // (cond (test expr) ...)
    disp_val clauses = disp_cdr(expr);
    while (NN(clauses) && T(clauses) == FLAG_CONS) {
        disp_val clause = disp_car(clauses);
        if (T(clause) != FLAG_CONS) {
            ERET(NIL, "cond: malformed clause");
        }
        disp_val test = disp_eval(env, disp_car(clause));
        if (NE(test, NIL)) {
            // evaluate the rest of the clause (one or more expressions)
            disp_val body = disp_cdr(clause);
            if (N(body) || T(body) != FLAG_CONS) return NIL;
            // evaluate all but return the last
            disp_val last = NIL;
            while (NN(body) && T(body) == FLAG_CONS) {
                last = disp_eval(env, disp_car(body));
                body = disp_cdr(body);
            }
            return last;
        }
        clauses = disp_cdr(clauses);
    }
    return NIL;
}

// --- while ---
static disp_val while_builtin(disp_env_t *env, disp_val expr) {
    // (while test body...)
    disp_val rest = disp_cdr(expr);
    if (N(rest) || T(rest) != FLAG_CONS) ERET(NIL, "while: missing test");
    disp_val test_form = disp_car(rest);
    disp_val body = disp_cdr(rest);
    if (N(body)) return NIL;
    disp_val last = NIL;
    while (1) {
        disp_val cond = disp_eval(env, test_form);
        if (E(cond, NIL)) break;
        // evaluate body forms
        disp_val body_it = body;
        while (NN(body_it) && T(body_it) == FLAG_CONS) {
            last = disp_eval(env, disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    return last;
}

// --- and ---
static disp_val and_builtin(disp_env_t *env, disp_val expr) {
    disp_val args = disp_cdr(expr);
    disp_val last = TRUE;
    while (NN(args) && T(args) == FLAG_CONS) {
        disp_val val = disp_eval(env, disp_car(args));
        if (E(val, NIL)) return NIL;
        last = val;
        args = disp_cdr(args);
    }
    return last;
}

// --- or ---
static disp_val or_builtin(disp_env_t *env, disp_val expr) {
    disp_val args = disp_cdr(expr);
    while (NN(args) && T(args) == FLAG_CONS) {
        disp_val val = disp_eval(env, disp_car(args));
        if (NE(val, NIL)) return val;
            args = disp_cdr(args);
        }
    return NIL;
}

// --- begin ---
static disp_val begin_builtin(disp_env_t *env, disp_val expr) {
    // (begin expr1 expr2 ...)
    disp_val body = disp_cdr(expr);
    if (N(body)) return NIL;           // 空 begin 返回 nil
    // 顺序求值所有表达式，返回最后一个的值
    disp_val last = NIL;
    while (NN(body) && T(body) == FLAG_CONS) {
        last = disp_eval(env, disp_car(body));
        body = disp_cdr(body);
    }
    return last;
}

static disp_val repeat_builtin(disp_env_t *env, disp_val expr) {
    // 语法: (repeat count body ...)
    disp_val rest = disp_cdr(expr);
    if (N(rest) || T(rest) != FLAG_CONS) 
        ERET(NIL, "repeat: missing count");
    disp_val count_val = disp_eval(env, disp_car(rest));
    long n = (T(count_val) == FLAG_INT) ? disp_get_int(count_val) 
            : (T(count_val) == FLAG_LONG || T(count_val) == TAG_LONG) ? disp_get_long(count_val) 
            : 0;
    if (n <= 0) return NIL;
    disp_val body = disp_cdr(rest);
    if (N(body)) return NIL;
    disp_val last = NIL;
    for (long i = 0; i < n; i++) {
        disp_val body_it = body;
        while (NN(body_it) && T(body_it) == FLAG_CONS) {
            last = disp_eval(env, disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    return NN(last) ? last : NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    REG("if"      , MKB(if_builtin     , "<#if>"     ), 1);
    REG("cond"    , MKB(cond_builtin   , "<#cond>"   ), 1);
    REG("while"   , MKB(while_builtin  , "<#while>"  ), 1);
    REG("and"     , MKB(and_builtin    , "<#and>"    ), 1);
    REG("or"      , MKB(or_builtin     , "<#or>"     ), 1);
    REG("begin"   , MKB(begin_builtin  , "<#begin>"  ), 1);
    REG("progn"   , MKB(begin_builtin  , "<#progn>"  ), 1);
    REG("repeat"  , MKB(repeat_builtin , "<#repeat>" ), 1);
}
