
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
static disp_box if_builtin(disp_scope_t *scope, disp_box expr) {
    // (if cond then [else])
    disp_box cadr = disp_cdr(expr);
    if (!cadr || T(cadr) != DISP_CONS) ERET(NIL, "if: missing condition");
    disp_box cond = disp_eval(scope, disp_car(cadr));
    disp_box then_rest = disp_cdr(cadr);
    if (!then_rest || T(then_rest) != DISP_CONS) ERET(NIL, "if: missing then clause");
    if (cond != NIL) {
        return disp_eval(scope, disp_car(then_rest));
    } else {
        disp_box else_rest = disp_cdr(then_rest);
        if (else_rest && T(else_rest) == DISP_CONS)
            return disp_eval(scope, disp_car(else_rest));
        else
            return NIL;
    }
}

// --- cond ---
static disp_box cond_builtin(disp_scope_t *scope, disp_box expr) {
    // (cond (test expr) ...)
    disp_box clauses = disp_cdr(expr);
    while (clauses && T(clauses) == DISP_CONS) {
        disp_box clause = disp_car(clauses);
        if (T(clause) != DISP_CONS) {
            ERET(NIL, "cond: malformed clause");
        }
        disp_box test = disp_eval(scope, disp_car(clause));
        if (test != NIL) {
            // evaluate the rest of the clause (one or more expressions)
            disp_box body = disp_cdr(clause);
            if (!body || T(body) != DISP_CONS) return NIL;
            // evaluate all but return the last
            disp_box last = NIL;
            while (body && T(body) == DISP_CONS) {
                last = disp_eval(scope, disp_car(body));
                body = disp_cdr(body);
            }
            return last;
        }
        clauses = disp_cdr(clauses);
    }
    return NIL;
}

// --- while ---
static disp_box while_builtin(disp_scope_t *scope, disp_box expr) {
    // (while test body...)
    disp_box rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "while: missing test");
    disp_box test_form = disp_car(rest);
    disp_box body = disp_cdr(rest);
    if (!body) return NIL;
    disp_box last = NIL;
    while (1) {
        disp_box cond = disp_eval(scope, test_form);
        if (cond == NIL) break;
        // evaluate body forms
        disp_box body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            last = disp_eval(scope, disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    return last;
}

// --- and ---
static disp_box and_builtin(disp_scope_t *scope, disp_box expr) {
    disp_box args = disp_cdr(expr);
    disp_box last = TRUE;
    while (args && T(args) == DISP_CONS) {
        disp_box val = disp_eval(scope, disp_car(args));
        if (val == NIL) return NIL;
        last = val;
        args = disp_cdr(args);
    }
    return last;
}

// --- or ---
static disp_box or_builtin(disp_scope_t *scope, disp_box expr) {
    disp_box args = disp_cdr(expr);
    while (args && T(args) == DISP_CONS) {
        disp_box val = disp_eval(scope, disp_car(args));
        if (val != NIL) return val;
            args = disp_cdr(args);
        }
    return NIL;
}

// --- begin ---
static disp_box begin_builtin(disp_scope_t *scope, disp_box expr) {
    // (begin expr1 expr2 ...)
    disp_box body = disp_cdr(expr);
    if (!body) return NIL;           // 空 begin 返回 nil
    // 顺序求值所有表达式，返回最后一个的值
    disp_box last = NIL;
    while (body && T(body) == DISP_CONS) {
        last = disp_eval(scope, disp_car(body));
        body = disp_cdr(body);
    }
    return last;
}

static disp_box repeat_builtin(disp_scope_t *scope, disp_box expr) {
    // 语法: (repeat count body ...)
    disp_box rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) 
        ERET(NIL, "repeat: missing count");
    disp_box count_val = disp_eval(scope, disp_car(rest));
    long n = (T(count_val) == DISP_INT) ? disp_get_int(count_val) 
            : (T(count_val) == DISP_LONG) ? disp_get_long(count_val) 
            : 0;
    if (n <= 0) return NIL;
    disp_box body = disp_cdr(rest);
    if (!body) return NIL;
    disp_box last = NIL;
    for (long i = 0; i < n; i++) {
        disp_box body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            last = disp_eval(scope, disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    return last ? last : NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("if"      , MKB(if_builtin     , "<#if>"     ), 1);
    DEF("cond"    , MKB(cond_builtin   , "<#cond>"   ), 1);
    DEF("while"   , MKB(while_builtin  , "<#while>"  ), 1);
    DEF("and"     , MKB(and_builtin    , "<#and>"    ), 1);
    DEF("or"      , MKB(or_builtin     , "<#or>"     ), 1);
    DEF("begin"   , MKB(begin_builtin  , "<#begin>"  ), 1);
    DEF("progn"   , MKB(begin_builtin  , "<#progn>"  ), 1);
    DEF("repeat"  , MKB(repeat_builtin , "<#repeat>" ), 1);
}
