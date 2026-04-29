
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

static disp_val* append(disp_val *a, disp_val *b) {
    if (!a || a == NIL) return b;
    if (T(a) != DISP_CONS) ERET(NIL, "append: first argument must be a list");
    return disp_make_cons(disp_car(a), append(disp_cdr(a), b));
}

static disp_val* append_builtin(disp_val *expr) {
    disp_val *a = disp_cdr(expr);
    if (!a || a == NIL) return NIL;
    disp_val *b = disp_cdr(a);
   a = disp_car(a);
    return append(a, b);
}

static disp_val* append_syscall(disp_val **args, int count) {
    if (count == 0) return NIL;
    // 结果列表的头和尾
    disp_val *result = NIL;
    disp_val *tail = NIL;
    for (int i = 0; i < count; i++) {
        disp_val *lst = args[i];
        // 检查类型
        if (lst != NIL && T(lst) != DISP_CONS) {
            ERET(NIL, "append: argument %d is not a list", i);
        }
        // 遍历 lst，将其元素复制到结果末尾
        for (disp_val *p = lst; p != NIL && T(p) == DISP_CONS; p = disp_cdr(p)) {
            disp_val *new_cons = disp_make_cons(disp_car(p), NIL);
            if (result == NIL) {
                result = new_cons;
                tail = new_cons;
            } else {
                disp_set_cdr(tail, new_cons);
                tail = new_cons;
            }
        }
    }
    return result ? result : NIL;
}

// --- quote ---
// 特殊处理 quote
static disp_val* quote_builtin(disp_val *expr) {
    // 返回第二个元素，不求值
    disp_val *quoted = disp_cdr(expr);
    if (quoted && T(quoted) == DISP_CONS) {
        return disp_car(quoted);
    } else {
        ERET(NIL, "quote: missing argument");
    }
}

/*
*/


/* * Helper: Checks if a list starts with a specific symbol (e.g., "unquote")
 */
static int is_tag(disp_val *expr, disp_val *tag) {
    if (!expr || expr == NIL || T(expr) != DISP_CONS) 
        return 0;
    disp_val *head = disp_car(expr);
    // Ensure the head is a symbol and matches the tag
    return (head && T(head) == DISP_SYMBOL && head == tag);
}

static disp_val* unquote(disp_val *expr) {
    while (is_tag(expr, UNQUOTE)) {
        expr = disp_car(disp_cdr(expr));
    }
    return expr;
}

static int is_prim(disp_val *expr) {
    if (!expr || T(expr) == DISP_CONS || T(expr) != DISP_SYMBOL) return 0;
    return 
        expr == NIL ||
        T(expr) == DISP_BYTE   ||
        T(expr) == DISP_SHORT  ||
        T(expr) == DISP_INT    ||
        T(expr) == DISP_LONG   ||
        T(expr) == DISP_FLOAT  ||
        T(expr) == DISP_DOUBLE ||
        T(expr) == DISP_STRING;
}

static disp_val * SPLICE_MARK;
static disp_val *expand_list(disp_val *list, int level);

/*
 * The main expansion engine
 */
static disp_val* qq_expand(disp_val* expr, int level) {
    //if (expr == NIL) return disp_make_cons(QUOTE, disp_make_cons(NIL, NIL));
    if(is_prim(expr)) return expr;

    // If it's a basic atom, return it quoted so it evaluates to itself
    if (T(expr) != DISP_CONS) {
        return disp_make_cons(QUOTE, disp_make_cons(expr, NIL));
    }
    
    if (is_tag(expr, QUASIQUOTE)) {
        disp_val* inner = disp_car(disp_cdr(expr));
        return disp_make_cons(LIST, 
            disp_make_cons(disp_make_cons(QUOTE, disp_make_cons(QUASIQUOTE, NIL)),
            disp_make_cons(qq_expand(inner, level + 1), NIL)));
    }
    else if (is_tag(expr, UNQUOTE)) {
        disp_val* inner = disp_car(disp_cdr(expr));
        if (level == 1) {
            // Level 1: This is the level we are currently expanding.
            // Return the expression directly to be evaluated.
            return inner; 
        } else if (is_tag(inner, UNQUOTE)) {
            return unquote(inner); 
        } else {
            // Level > 1: This belongs to a deeper quasiquote.
            // We expand the inner part, then wrap it back in UNQUOTE.
            // FIX: We need to make sure the result is treated as a list 
            // construction so that it produces (unquote 5) instead of (unquote (quote 5)).
            return disp_make_cons(LIST, 
                disp_make_cons(disp_make_cons(QUOTE, disp_make_cons(UNQUOTE, NIL)),
                disp_make_cons(qq_expand(inner, level - 1), NIL)));
        }
    }    
    
    if (is_tag(expr, UNQUOTE_SPLICING)) {
        disp_val* inner = disp_car(disp_cdr(expr));
        if (level == 1) {
            // Return a special marker for expand_list to handle
            return disp_make_cons(SPLICE_MARK, disp_make_cons(inner, NIL));
        } else {
            return disp_make_cons(LIST, 
                disp_make_cons(disp_make_cons(QUOTE, disp_make_cons(UNQUOTE_SPLICING, NIL)),
                disp_make_cons(qq_expand(inner, level - 1), NIL)));
        }
    }
    
    return expand_list(expr, level);
}

/*
 * Expands elements within a list, handling unquote-splicing
 */
static disp_val* expand_list(disp_val* list, int level) {
    if (list == NIL) return disp_make_cons(QUOTE, disp_make_cons(NIL, NIL));

    disp_val* head = disp_car(list);
    disp_val* expanded_head = qq_expand(head, level);
    disp_val* tail = expand_list(disp_cdr(list), level);

    // Check for the (SPLICE_MARK inner)
    if (T(expanded_head) == DISP_CONS && disp_car(expanded_head) == SPLICE_MARK) {
        disp_val* inner = disp_car(disp_cdr(expanded_head));
        // Result: (APPEND inner tail)
        return disp_make_cons(APPEND,
                 disp_make_cons(inner,
                   disp_make_cons(tail, NIL)));
    } else {
        // Result: (CONS expanded_head tail)
        return disp_make_cons(CONS,
                 disp_make_cons(expanded_head,
                   disp_make_cons(tail, NIL)));
    }
}

/*
 *Level Management: The level ensures that nested backticks only unquote at the appropriate depth
 */
static disp_val* quasiquote_builtin(disp_val *expr) {
    disp_val *cdr = disp_cdr(expr);
    if (!cdr || T(cdr) != DISP_CONS) return NIL;
    
    disp_val *tmpl = disp_car(cdr);
    
    // 1. Generate the construction code (e.g., (CONS 1 (CONS 5 ...)))
    disp_val *expanded_code = qq_expand(tmpl, 1);
    
    // 2. Evaluate that code to get the final list
    return disp_eval(expanded_code);
}

/*
 * (unquote x) -> evaluates x and returns the result
 */
static disp_val* unquote_builtin(disp_val *expr) {
    disp_val *cdr = disp_cdr(expr);
    if (!cdr) ERET(NIL, "unquote: expects one argument");
    if (T(cdr) != DISP_CONS) return disp_eval(cdr);
    if (disp_cdr(cdr) != NIL)
        ERET(NIL, "unquote: expects exactly one argument");
    return disp_eval(disp_car(cdr));
}

/*
 * (unquote-splice x) -> evaluates x, which MUST result in a list
 */
static disp_val* unquote_splicing_builtin(disp_val *expr) {
    // expr contains the expression to unquote-splicing
    disp_val *cdr = disp_cdr(expr);
    if (!cdr || T(cdr) != DISP_CONS)
        ERET(NIL, "unquote-splicing: expects cons argument");

    disp_val *result = disp_eval(cdr);

    // Requirement: result of splicing must be a list
    if (T(result) != DISP_CONS)
        ERET(NIL, "unquote-splicing: expects cons result");

    return result;
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

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {

    SPLICE_MARK = disp_intern_symbol("splice-mark");

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
    DEF("append"  , MKF(append_syscall , "<append>"  ), 1);
    DEF("append0" , MKB(append_builtin , "<#append>" ), 1);
    DEF("quote"   , MKB(quote_builtin  , "<#quote>"  ), 1);
    DEF("quasiquote"       , MKB(quasiquote_builtin        , "<#quasiquote>"      ), 1);
    DEF("unquote"          , MKB(unquote_builtin           , "<#unquote>"         ), 1);
    DEF("unquote-splicing" , MKB(unquote_splicing_builtin  , "<#unquote-splicing>"), 1);
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
}
