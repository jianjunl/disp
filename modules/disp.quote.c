
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

static disp_val append(disp_val a, disp_val b) {
    if (N(a) || E(a, NIL)) return b;
    if (T(a) != FLAG_CONS) ERET(NIL, "append: first argument must be a list");
    return disp_make_cons(disp_car(a), append(disp_cdr(a), b));
}

static disp_val append_builtin(disp_env_t *env, disp_val expr) {
    ((void)env);
    disp_val a = disp_cdr(expr);
    if (N(a) || E(a, NIL)) return NIL;
    disp_val b = disp_cdr(a);
   a = disp_car(a);
    return append(a, b);
}

static disp_val append_syscall(disp_val *args, int count) {
    if (count == 0) return NIL;
    // 结果列表的头和尾
    disp_val result = NIL;
    disp_val tail = NIL;
    for (int i = 0; i < count; i++) {
        disp_val lst = args[i];
        // 检查类型
        if (NE(lst, NIL) && T(lst) != FLAG_CONS) {
            ERET(NIL, "append: argument %d is not a list", i);
        }
        // 遍历 lst，将其元素复制到结果末尾
        for (disp_val p = lst; NE(p, NIL) && T(p) == FLAG_CONS; p = disp_cdr(p)) {
            disp_val new_cons = disp_make_cons(disp_car(p), NIL);
            if (E(result, NIL)) {
                result = new_cons;
                tail = new_cons;
            } else {
                disp_set_cdr(tail, new_cons);
                tail = new_cons;
            }
        }
    }
    return NN(result) ? result : NIL;
}

// --- quote ---
// 特殊处理 quote
static disp_val quote_builtin(disp_env_t *env, disp_val expr) {
    ((void)env);
    // 返回第二个元素，不求值
    disp_val quoted = disp_cdr(expr);
    if (NN(quoted) && T(quoted) == FLAG_CONS) {
        return disp_car(quoted);
    } else {
        ERET(NIL, "quote: missing argument");
    }
}

/*
*/


/* * Helper: Checks if a list starts with a specific symbol (e.g., "unquote")
 */
static int is_tag(disp_val expr, disp_sid tag) {
    if (N(expr) || E(expr, NIL) || T(expr) != FLAG_CONS) 
        return 0;
    disp_val head = disp_car(expr);
    // Ensure the head is a symbol and matches the tag
    return (NN(head) && T(head) == FLAG_SYMBOL && SYM_ID(head).id == tag.id);
}

static disp_val unquote(disp_val expr) {
    while (is_tag(expr, UNQUOTE)) {
        expr = disp_car(disp_cdr(expr));
    }
    return expr;
}

static int is_prim(disp_val expr) {
    if (N(expr) || T(expr) == FLAG_CONS || T(expr) != FLAG_SYMBOL) return 0;
    return 
        E(expr, NIL) ||
        T(expr) == FLAG_BYTE   ||
        T(expr) == FLAG_SHORT  ||
        T(expr) == FLAG_INT    ||
        T(expr) == FLAG_LONG   ||
        T(expr) == TAG_LONG    ||
        T(expr) == FLAG_FLOAT  ||
        T(expr) == FLAG_DOUBLE ||
        T(expr) == FLAG_STRING;
}

static disp_val SPLICE_MARK;
static disp_val expand_list(disp_val list, int level);

/*
 * The main expansion engine
 */
static disp_val qq_expand(disp_val expr, int level) {
    if(is_prim(expr)) return expr;

    // If it's a basic atom, return it quoted so it evaluates to itself
    if (T(expr) != FLAG_CONS) {
        return disp_make_cons(GLOBAL(QUOTE), disp_make_cons(expr, NIL));
    }
    
    if (is_tag(expr, QUASIQUOTE)) {
        disp_val inner = disp_car(disp_cdr(expr));
        return disp_make_cons(GLOBAL(LIST), 
            disp_make_cons(disp_make_cons(GLOBAL(QUOTE), disp_make_cons(GLOBAL(QUASIQUOTE), NIL)),
            disp_make_cons(qq_expand(inner, level + 1), NIL)));
    }
    else if (is_tag(expr, UNQUOTE)) {
        disp_val inner = disp_car(disp_cdr(expr));
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
            return disp_make_cons(GLOBAL(LIST), 
                disp_make_cons(disp_make_cons(GLOBAL(QUOTE), disp_make_cons(GLOBAL(UNQUOTE), NIL)),
                disp_make_cons(qq_expand(inner, level - 1), NIL)));
        }
    }    
    
    if (is_tag(expr, UNQUOTE_SPLICING)) {
        disp_val inner = disp_car(disp_cdr(expr));
        if (level == 1) {
            // Return a special marker for expand_list to handle
            return disp_make_cons(SPLICE_MARK, disp_make_cons(inner, NIL));
        } else {
            return disp_make_cons(GLOBAL(LIST), 
                disp_make_cons(disp_make_cons(GLOBAL(QUOTE), disp_make_cons(GLOBAL(UNQUOTE_SPLICING), NIL)),
                disp_make_cons(qq_expand(inner, level - 1), NIL)));
        }
    }
    
    return expand_list(expr, level);
}

/*
 * Expands elements within a list, handling unquote-splicing
 */
static disp_val expand_list(disp_val list, int level) {
    if (E(list, NIL)) return disp_make_cons(GLOBAL(QUOTE), disp_make_cons(NIL, NIL));

    disp_val head = disp_car(list);
    disp_val expanded_head = qq_expand(head, level);
    disp_val tail = expand_list(disp_cdr(list), level);

    // Check for the (SPLICE_MARK inner)
    if (T(expanded_head) == FLAG_CONS && E(disp_car(expanded_head), SPLICE_MARK)) {
        disp_val inner = disp_car(disp_cdr(expanded_head));
        // Result: (APPEND inner tail)
        return disp_make_cons(GLOBAL(APPEND),
                 disp_make_cons(inner,
                   disp_make_cons(tail, NIL)));
    } else {
        // Result: (CONS expanded_head tail)
        return disp_make_cons(GLOBAL(CONS),
                 disp_make_cons(expanded_head,
                   disp_make_cons(tail, NIL)));
    }
}

/*
 *Level Management: The level ensures that nested backticks only unquote at the appropriate depth
 */
static disp_val quasiquote_builtin(disp_env_t *env, disp_val expr) {
    disp_val cdr = disp_cdr(expr);
    if (N(cdr) || T(cdr) != FLAG_CONS) return NIL;
    
    disp_val tmpl = disp_car(cdr);
    
    // 1. Generate the construction code (e.g., (CONS 1 (CONS 5 ...)))
    disp_val expanded_code = qq_expand(tmpl, 1);
    
    // 2. Evaluate that code to get the final list
    return disp_eval(env, expanded_code);
}

/*
 * (unquote x) -> evaluates x and returns the result
 */
static disp_val unquote_builtin(disp_env_t *env, disp_val expr) {
    disp_val cdr = disp_cdr(expr);
    if (N(cdr)) ERET(NIL, "unquote: expects one argument");
    if (T(cdr) != FLAG_CONS) return disp_eval(env, cdr);
    if (NE(disp_cdr(cdr), NIL))
        ERET(NIL, "unquote: expects exactly one argument");
    return disp_eval(env, disp_car(cdr));
}

/*
 * (unquote-splice x) -> evaluates x, which MUST result in a list
 */
static disp_val unquote_splicing_builtin(disp_env_t *env, disp_val expr) {
    // expr contains the expression to unquote-splicing
    disp_val cdr = disp_cdr(expr);
    if (N(cdr) || T(cdr) != FLAG_CONS)
        ERET(NIL, "unquote-splicing: expects cons argument");

    disp_val result = disp_eval(env, cdr);

    // Requirement: result of splicing must be a list
    if (T(result) != FLAG_CONS)
        ERET(NIL, "unquote-splicing: expects cons result");

    return result;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {

    SPLICE_MARK = disp_intern_symbol_by_name(disp_global_env, "splice-mark");

    REG("append"  , MKF(append_syscall , "<append>"  ), 1);
    REG("append0" , MKB(append_builtin , "<#append>" ), 1);
    REG("quote"   , MKB(quote_builtin  , "<#quote>"  ), 1);
    REG("quasiquote"       , MKB(quasiquote_builtin        , "<#quasiquote>"      ), 1);
    REG("unquote"          , MKB(unquote_builtin           , "<#unquote>"         ), 1);
    REG("unquote-splicing" , MKB(unquote_splicing_builtin  , "<#unquote-splicing>"), 1);
}
