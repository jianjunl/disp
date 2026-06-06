
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
    if (N(cadr) || T(cadr) != FLAG_CONS) ERET(NIL, "set!: missing symbol");
    disp_val sym = disp_car(cadr);
    if (T(sym) != FLAG_SYMBOL) ERET(NIL, "set!: first argument must be a symbol");
    uint64_t id = SYM_ID(sym);
    
    disp_val rest = disp_cdr(cadr);
    if (N(rest) || T(rest) != FLAG_CONS) ERET(NIL, "set!: missing expression");
    disp_val new_value = disp_eval(env, disp_car(rest));
    
    disp_val found_sym = disp_find_symbol(env, id);
    if (N(found_sym)) {
        ERET(NIL, "set!: undefined variable '%s'", disp_get_name(id));
    }
    disp_set_symbol_value_unlock(env, found_sym, new_value);
    return new_value;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("set!"    , MKB(setq_builtin   , "<#set!>"   ), 1);
    DEF("setq"    , MKB(setq_builtin   , "<#set!>"   ), 1);
}
