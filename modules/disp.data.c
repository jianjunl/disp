
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

// List primitives
static disp_val* cons_syscall(disp_val **args, int count) {
    if (count != 2) {
        ERET(NIL, "cons expects two arguments");
    }
    return disp_make_cons(args[0], args[1]);
}

static disp_val* car_syscall(disp_val **args, int count) {
    if (count != 1) {
        ERET(NIL, "car expects one argument");
    }
    return disp_car(args[0]);
}

static disp_val* cdr_syscall(disp_val **args, int count) {
    if (count != 1) {
        ERET(NIL, "cdr expects one argument");
    }
    return disp_cdr(args[0]);
}

static disp_val* list_syscall(disp_val **args, int count) {
    disp_val *result = NIL;
    for (int i = count - 1; i >= 0; i--)
        result = disp_make_cons(args[i], result);
    return result;
}

// --- not ---
static disp_val* not_syscall(disp_val **args, int count) {
    if (count != 1) {
        ERET(NIL, "not: expected one argument");
    }
    return (args[0] == NIL) ? TRUE : NIL;
}

// --- eq? --- (simple pointer comparison)
static disp_val* eqp_syscall(disp_val **args, int count) {
    if (count != 2) {
        ERET(NIL, "eq?: expected two arguments");
    }
    return (args[0] == args[1]) ? TRUE : NIL;
}

// --- < --- (numeric comparison)
static disp_val* lt_syscall(disp_val **args, int count) {
    if (count != 2) {
        ERET(NIL, "<: expected two arguments");
    }
    double a = 0, b = 0;
    // convert both to double for simplicity
    switch (T(args[0])) {
        case DISP_INT: a = disp_get_int(args[0]); break;
        case DISP_LONG: a = disp_get_long(args[0]); break;
        case DISP_FLOAT: a = disp_get_float(args[0]); break;
        case DISP_DOUBLE: a = disp_get_double(args[0]); break;
        default: ERRO("<: left operand not numeric");
    }
    switch (T(args[1])) {
        case DISP_INT: b = disp_get_int(args[1]); break;
        case DISP_LONG: b = disp_get_long(args[1]); break;
        case DISP_FLOAT: b = disp_get_float(args[1]); break;
        case DISP_DOUBLE: b = disp_get_double(args[1]); break;
        default: ERRO("<: right operand not numeric");
    }
    return (a < b) ? TRUE : NIL;
}

static disp_val* gt_syscall(disp_val **args, int count) {
    if (count != 2) {
        ERET(NIL, ">: expected two arguments");
    }
    double a = 0, b = 0;
    switch (T(args[0])) {
        case DISP_INT: a = disp_get_int(args[0]); break;
        case DISP_LONG: a = disp_get_long(args[0]); break;
        case DISP_FLOAT: a = disp_get_float(args[0]); break;
        case DISP_DOUBLE: a = disp_get_double(args[0]); break;
        default: ERRO(">: left operand not numeric");
    }
    switch (T(args[1])) {
        case DISP_INT: b = disp_get_int(args[1]); break;
        case DISP_LONG: b = disp_get_long(args[1]); break;
        case DISP_FLOAT: b = disp_get_float(args[1]); break;
        case DISP_DOUBLE: b = disp_get_double(args[1]); break;
        default: ERRO(">: right operand not numeric");
    }
    return (a > b) ? TRUE : NIL;
}

static disp_val* eq_syscall(disp_val **args, int count) {
    if (count != 2) {
        ERET(NIL, "=: expected two arguments");
    }

    // Compare numbers, with type promotion
    long double a = 0, b = 0;
    disp_val *v = disp_eval(args[0]);
    switch (T(v)) {
        case DISP_INT: a = disp_get_int(v); break;
        case DISP_LONG: a = disp_get_long(v); break;
        case DISP_FLOAT: a = disp_get_float(v); break;
        case DISP_DOUBLE: a = disp_get_double(v); break;
        default: ERRO("=: left operand not numeric");
    }
    v = disp_eval(args[1]);
    switch (T(v)) {
        case DISP_INT: b = disp_get_int(v); break;
        case DISP_LONG: b = disp_get_long(v); break;
        case DISP_FLOAT: b = disp_get_float(v); break;
        case DISP_DOUBLE: b = disp_get_double(v); break;
        default: ERRO("=: right operand not numeric");
    }
    return (a == b) ? TRUE : NIL;
}

// --- eval ---
static disp_val* eval_syscall(disp_val **args, int count) {
    if (count != 1) {
        ERET(NIL, "eval: expected one argument");
    }
    // 对参数表达式进行求值
    return disp_eval(args[0]);
}

/* ----- 类型谓词 ----- */
static disp_val* nullp_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "null?: expects one argument");
    return (args[0] == NIL) ? TRUE : NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* symbolp_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "symbol?: expects one argument");
    return (T(args[0]) == DISP_SYMBOL) ? TRUE : NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* stringp_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "string?: expects one argument");
    return (T(args[0]) == DISP_STRING) ? TRUE : NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* bytep_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "byte?: expects one argument");
    return (T(args[0]) == DISP_BYTE) ? TRUE : NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* shortp_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "short?: expects one argument");
    return (T(args[0]) == DISP_SHORT) ? TRUE : NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* intp_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "int?: expects one argument");
    return (T(args[0]) == DISP_INT) ? TRUE : NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* longp_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "long?: expects one argument");
    return (T(args[0]) == DISP_LONG) ? TRUE : NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* floatp_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "float?: expects one argument");
    return (T(args[0]) == DISP_FLOAT) ? TRUE : NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* doublep_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "double?: expects one argument");
    return (T(args[0]) == DISP_DOUBLE) ? TRUE : NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* integerp_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "integer?: expects one argument");
    if (
        T(args[0]) == DISP_BYTE   ||
        T(args[0]) == DISP_SHORT  ||
        T(args[0]) == DISP_INT    ||
        T(args[0]) == DISP_LONG
    ) return TRUE;
    return NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* decimalp_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "decimal?: expects one argument");
    if (
        T(args[0]) == DISP_BYTE   ||
        T(args[0]) == DISP_SHORT  ||
        T(args[0]) == DISP_INT    ||
        T(args[0]) == DISP_LONG   ||
        T(args[0]) == DISP_FLOAT  ||
        T(args[0]) == DISP_DOUBLE
    ) return TRUE;
    return NIL;
}

/* ----- 类型谓词 ----- */
static disp_val* numberp_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "number?: expects one argument");
    if (
        T(args[0]) == DISP_BYTE   ||
        T(args[0]) == DISP_SHORT  ||
        T(args[0]) == DISP_INT    ||
        T(args[0]) == DISP_LONG   ||
        T(args[0]) == DISP_FLOAT  ||
        T(args[0]) == DISP_DOUBLE
    ) return TRUE;
    return NIL;
}

/* ----- set-car! ----- */
static disp_val* set_car_syscall(disp_val **args, int count) {
    if (count != 2) {
        ERET(NIL, "set-car! expects two arguments (cons new-value)");
    }
    if (T(args[0]) != DISP_CONS) {
        ERET(NIL, "set-car!: first argument must be a cons cell");
    }
    disp_set_car(args[0], args[1]);
    return args[1];
}

/* ----- set-cdr! ----- */
static disp_val* set_cdr_syscall(disp_val **args, int count) {
    if (count != 2) {
        ERET(NIL, "set-cdr! expects two arguments (cons new-value)");
    }
    if (T(args[0]) != DISP_CONS) {
        ERET(NIL, "set-cdr!: first argument must be a cons cell");
    }
    disp_set_cdr(args[0], args[1]);
    return args[1];
}

/*
 * 通用值相等比较（equal）
 * 递归比较内容，支持 NIL、数值、字符串、符号、cons。
 * 为防止循环结构，设置递归深度上限。
 */
static disp_val* equal_syscall(disp_val **args, int count) {
    if (count != 2) {
        ERET(NIL, "equal: expected two arguments");
    }

    disp_val *a = args[0];
    disp_val *b = args[1];

    /* 递归深度保护（简单实现，避免无限循环） */
    static _Thread_local int depth = 0;
    if (depth > 1000) ERET(NIL, "equal: exceeded recursion depth");

    /* 比较 NIL */
    if (a == NIL && b == NIL) return TRUE;
    if (a == NIL || b == NIL) return NIL;

    /* 类型不同，快速失败 */
    if (T(a) != T(b)) return NIL;

    switch (T(a)) {
        case DISP_BYTE:   return (disp_get_byte(a)   == disp_get_byte(b))   ? TRUE : NIL;
        case DISP_SHORT:  return (disp_get_short(a)  == disp_get_short(b))  ? TRUE : NIL;
        case DISP_INT:    return (disp_get_int(a)    == disp_get_int(b))    ? TRUE : NIL;
        case DISP_LONG:   return (disp_get_long(a)   == disp_get_long(b))   ? TRUE : NIL;
        case DISP_FLOAT:  return (disp_get_float(a)  == disp_get_float(b))  ? TRUE : NIL;
        case DISP_DOUBLE: return (disp_get_double(a) == disp_get_double(b)) ? TRUE : NIL;
        case DISP_STRING: {
            const char *sa = disp_get_str(a);
            const char *sb = disp_get_str(b);
            if (!sa || !sb) return NIL;
            return (strcmp(sa, sb) == 0) ? TRUE : NIL;
        }
        case DISP_SYMBOL: {
            const char *na = disp_get_symbol_name(a);
            const char *nb = disp_get_symbol_name(b);
            if (!na || !nb) return NIL;
            return (strcmp(na, nb) == 0) ? TRUE : NIL;
        }
        case DISP_CONS: {
            depth++;
            disp_val *car_eq = equal_syscall((disp_val*[]){disp_car(a), disp_car(b)}, 2);
            if (car_eq == NIL) { depth--; return NIL; }
            disp_val *cdr_eq = equal_syscall((disp_val*[]){disp_cdr(a), disp_cdr(b)}, 2);
            depth--;
            return cdr_eq;
        }
        default:
            /* 其他类型（如闭包、文件等）不提供值比较，返回 NIL */
            return NIL;
    }
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {

    DEF("cons"    , MKF(cons_syscall   , "<cons>"    ), 1);
    DEF("car"     , MKF(car_syscall    , "<car>"     ), 1);
    DEF("cdr"     , MKF(cdr_syscall    , "<cdr>"     ), 1);
    DEF("list"    , MKF(list_syscall   , "<list>"    ), 1);
    DEF("not"     , MKF(not_syscall    , "<not>"     ), 1);
    DEF("eq?"     , MKF(eqp_syscall    , "<eq?>"     ), 1);
    DEF("<"       , MKF(lt_syscall     , "<lt>"      ), 1);
    DEF(">"       , MKF(gt_syscall     , "<gt>"      ), 1);
    DEF("="       , MKF(eq_syscall     , "<eq>"      ), 1);
    DEF("eval"    , MKF(eval_syscall   , "<eval>"    ), 1);
    DEF("null?"   , MKF(nullp_syscall  , "<null?>"   ), 1);
    DEF("symbol?" , MKF(symbolp_syscall, "<symbol?>" ), 1);
    DEF("string?" , MKF(stringp_syscall, "<string?>" ), 1);
    DEF("byte?"   , MKF(bytep_syscall  , "<byte?>"   ), 1);
    DEF("char?"   , MKF(bytep_syscall  , "<char?>"   ), 1);
    DEF("short?"  , MKF(shortp_syscall , "<short?>"  ), 1);
    DEF("int?"    , MKF(intp_syscall   , "<int?>"    ), 1);
    DEF("long?"   , MKF(longp_syscall  , "<long?>"   ), 1);
    DEF("float?"  , MKF(floatp_syscall , "<float?>"  ), 1);
    DEF("double?" , MKF(doublep_syscall  , "<double?>"  ), 1);
    DEF("integer?", MKF(integerp_syscall , "<integer?>" ), 1);
    DEF("decimal?", MKF(decimalp_syscall , "<decimal?>" ), 1);
    DEF("number?" , MKF(numberp_syscall  , "<number?>"  ), 1);
    DEF("set-car!", MKF(set_car_syscall, "<set-car!>"), 1);
    DEF("set-cdr!", MKF(set_cdr_syscall, "<set-cdr!>"), 1);
    DEF("equal"   , MKF(equal_syscall  , "<equal>"   ), 1);
}
