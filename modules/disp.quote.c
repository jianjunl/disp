
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

static disp_box append(disp_box a, disp_box b) {
    if (!a || a == NIL) return b;
    if (T(a) != FLAG_CONS) ERET(NIL, "append: first argument must be a list");
    return disp_make_cons(disp_car(a), append(disp_cdr(a), b));
}

static disp_box append_builtin(disp_scope_t *scope, disp_box expr) {
    ((void)scope);
    disp_box a = disp_cdr(expr);
    if (!a || a == NIL) return NIL;
    disp_box b = disp_cdr(a);
   a = disp_car(a);
    return append(a, b);
}

static disp_box append_syscall(disp_box *args, int count) {
    if (count == 0) return NIL;
    // 结果列表的头和尾
    disp_box result = NIL;
    disp_box tail = NIL;
    for (int i = 0; i < count; i++) {
        disp_box lst = args[i];
        // 检查类型
        if (lst != NIL && T(lst) != FLAG_CONS) {
            ERET(NIL, "append: argument %d is not a list", i);
        }
        // 遍历 lst，将其元素复制到结果末尾
        for (disp_box p = lst; p != NIL && T(p) == FLAG_CONS; p = disp_cdr(p)) {
            disp_box new_cons = disp_make_cons(disp_car(p), NIL);
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
static disp_box quote_builtin(disp_scope_t *scope, disp_box expr) {
    ((void)scope);
    // 返回第二个元素，不求值
    disp_box quoted = disp_cdr(expr);
    if (quoted && T(quoted) == FLAG_CONS) {
        return disp_car(quoted);
    } else {
        ERET(NIL, "quote: missing argument");
    }
}

/*
*/


/* * Helper: Checks if a list starts with a specific symbol (e.g., "unquote")
 */
static int is_tag(disp_box expr, disp_box tag) {
    if (!expr || expr == NIL || T(expr) != FLAG_CONS) 
        return 0;
    disp_box head = disp_car(expr);
    // Ensure the head is a symbol and matches the tag
    return (head && T(head) == FLAG_SYMBOL && head == tag);
}

static disp_box unquote(disp_box expr) {
    while (is_tag(expr, UNQUOTE)) {
        expr = disp_car(disp_cdr(expr));
    }
    return expr;
}

static int is_prim(disp_box expr) {
    if (!expr || T(expr) == FLAG_CONS || T(expr) != FLAG_SYMBOL) return 0;
    return 
        expr == NIL ||
        T(expr) == FLAG_BYTE   ||
        T(expr) == FLAG_SHORT  ||
        T(expr) == FLAG_INT    ||
        T(expr) == FLAG_LONG   ||
        T(expr) == FLAG_FLOAT  ||
        T(expr) == FLAG_DOUBLE ||
        T(expr) == FLAG_STRING;
}

static disp_box  SPLICE_MARK;
static disp_box expand_list(disp_box list, int level);

/*
 * The main expansion engine
 */
static disp_box qq_expand(disp_box expr, int level) {
    //if (expr == NIL) return disp_make_cons(QUOTE, disp_make_cons(NIL, NIL));
    if(is_prim(expr)) return expr;

    // If it's a basic atom, return it quoted so it evaluates to itself
    if (T(expr) != FLAG_CONS) {
        return disp_make_cons(QUOTE, disp_make_cons(expr, NIL));
    }
    
    if (is_tag(expr, QUASIQUOTE)) {
        disp_box inner = disp_car(disp_cdr(expr));
        return disp_make_cons(LIST, 
            disp_make_cons(disp_make_cons(QUOTE, disp_make_cons(QUASIQUOTE, NIL)),
            disp_make_cons(qq_expand(inner, level + 1), NIL)));
    }
    else if (is_tag(expr, UNQUOTE)) {
        disp_box inner = disp_car(disp_cdr(expr));
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
        disp_box inner = disp_car(disp_cdr(expr));
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
static disp_box expand_list(disp_box list, int level) {
    if (list == NIL) return disp_make_cons(QUOTE, disp_make_cons(NIL, NIL));

    disp_box head = disp_car(list);
    disp_box expanded_head = qq_expand(head, level);
    disp_box tail = expand_list(disp_cdr(list), level);

    // Check for the (SPLICE_MARK inner)
    if (T(expanded_head) == FLAG_CONS && disp_car(expanded_head) == SPLICE_MARK) {
        disp_box inner = disp_car(disp_cdr(expanded_head));
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
static disp_box quasiquote_builtin(disp_scope_t *scope, disp_box expr) {
    disp_box cdr = disp_cdr(expr);
    if (!cdr || T(cdr) != FLAG_CONS) return NIL;
    
    disp_box tmpl = disp_car(cdr);
    
    // 1. Generate the construction code (e.g., (CONS 1 (CONS 5 ...)))
    disp_box expanded_code = qq_expand(tmpl, 1);
    
    // 2. Evaluate that code to get the final list
    return disp_eval(scope, expanded_code);
}

/*
 * (unquote x) -> evaluates x and returns the result
 */
static disp_box unquote_builtin(disp_scope_t *scope, disp_box expr) {
    disp_box cdr = disp_cdr(expr);
    if (!cdr) ERET(NIL, "unquote: expects one argument");
    if (T(cdr) != FLAG_CONS) return disp_eval(scope, cdr);
    if (disp_cdr(cdr) != NIL)
        ERET(NIL, "unquote: expects exactly one argument");
    return disp_eval(scope, disp_car(cdr));
}

/*
 * (unquote-splice x) -> evaluates x, which MUST result in a list
 */
static disp_box unquote_splicing_builtin(disp_scope_t *scope, disp_box expr) {
    // expr contains the expression to unquote-splicing
    disp_box cdr = disp_cdr(expr);
    if (!cdr || T(cdr) != FLAG_CONS)
        ERET(NIL, "unquote-splicing: expects cons argument");

    disp_box result = disp_eval(scope, cdr);

    // Requirement: result of splicing must be a list
    if (T(result) != FLAG_CONS)
        ERET(NIL, "unquote-splicing: expects cons result");

    return result;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {

    SPLICE_MARK = disp_intern_symbol(NULL, "splice-mark");

    DEF("append"  , MKF(append_syscall , "<append>"  ), 1);
    DEF("append0" , MKB(append_builtin , "<#append>" ), 1);
    DEF("quote"   , MKB(quote_builtin  , "<#quote>"  ), 1);
    DEF("quasiquote"       , MKB(quasiquote_builtin        , "<#quasiquote>"      ), 1);
    DEF("unquote"          , MKB(unquote_builtin           , "<#unquote>"         ), 1);
    DEF("unquote-splicing" , MKB(unquote_splicing_builtin  , "<#unquote-splicing>"), 1);
}
