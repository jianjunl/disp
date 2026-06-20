
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

#define DEF_TYPE_BUILTIN(name, converter, maker) \
static disp_val name##_builtin(disp_env_t *env, disp_val expr) { \
    disp_val cadr = disp_cdr(expr); \
    if (N(cadr) || T(cadr) != FLAG_CONS) \
        ERET(NIL, #name ": missing argument"); \
    disp_val sym = disp_car(cadr); \
    if (T(sym) != FLAG_SYMBOL) \
        ERET(NIL, #name ": argument must be a symbol"); \
    disp_val cur = disp_find_symbol(env, SYM_ID(sym)); \
    if (N(cur)) { \
        disp_val zero = maker(0); \
        disp_define_symbol(env, SYM_ID(sym), zero, 0); \
        return zero; \
    } else { \
        disp_val converted = converter(cur); \
        disp_set_symbol_value_unlock(env, sym, converted); \
        return converted; \
    } \
}

DEF_TYPE_BUILTIN(byte,   disp_convert_to_byte,   disp_make_byte)
DEF_TYPE_BUILTIN(short,  disp_convert_to_short,  disp_make_short)
DEF_TYPE_BUILTIN(int,    disp_convert_to_int,    disp_make_int)
DEF_TYPE_BUILTIN(long,   disp_convert_to_long,   disp_make_long)
DEF_TYPE_BUILTIN(float,  disp_convert_to_float,  disp_make_float)
DEF_TYPE_BUILTIN(double, disp_convert_to_double, disp_make_double)

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    REG("byte"    , MKB(byte_builtin   , "<#byte>"   ), 1);
    REG("short"   , MKB(short_builtin  , "<#short>"  ), 1);
    REG("int"     , MKB(int_builtin    , "<#int>"    ), 1);
    REG("long"    , MKB(long_builtin   , "<#long>"   ), 1);
    REG("float"   , MKB(float_builtin  , "<#float>"  ), 1);
    REG("double"  , MKB(double_builtin , "<#double>" ), 1);
}
