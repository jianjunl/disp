
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

// --- lambda ---
static disp_val lambda_builtin(disp_env_t *env, disp_val expr) {
    // (lambda (params) body...)
    disp_val rest = disp_cdr(expr);
    if (N(rest) || T(rest) != FLAG_CONS) ERET(NIL, "lambda: missing parameter list");
    disp_val params = disp_car(rest);
    disp_val body = disp_cdr(rest);
    if (N(body)) ERET(NIL, "lambda: missing body");
    return disp_make_closure(env, params, body, 0);
}

// --- macro ---
static disp_val macro_builtin(disp_env_t *env, disp_val expr) {
    // (macro (params) body...)
    disp_val rest = disp_cdr(expr);
    if (N(rest) || T(rest) != FLAG_CONS) ERET(NIL, "macro: missing parameter list");
    disp_val params = disp_car(rest);
    disp_val body = disp_cdr(rest);
    if (N(body)) ERET(NIL, "macro: missing body");
    return disp_make_macro(env, params, body, 0);
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    REG("lambda"  , MKB(lambda_builtin , "<#lambda>" ), 1);
    REG("macro"   , MKB(macro_builtin  , "<#macro>"  ), 1);
}
