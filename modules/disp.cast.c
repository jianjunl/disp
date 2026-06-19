
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

// --- type definition / coercion helpers ---
static disp_val convert_to_byte(disp_val v) {
    switch (T(v)) {
        case FLAG_BYTE:   return v;
        case FLAG_SHORT:  return disp_make_byte((char)disp_get_short(v));
        case FLAG_INT:    return disp_make_byte((char)disp_get_int(v));
        case FLAG_LONG:   return disp_make_byte((char)disp_get_long(v));
        case FLAG_FLOAT:  return disp_make_byte((char)disp_get_float(v));
        case FLAG_DOUBLE: return disp_make_byte((char)disp_get_double(v));
        default: ERET(NIL, "cannot convert to byte");
    }
}

static disp_val convert_to_short(disp_val v) {
    switch (T(v)) {
        case FLAG_BYTE:   return disp_make_short(disp_get_byte(v));
        case FLAG_SHORT:  return v;
        case FLAG_INT:    return disp_make_short((short)disp_get_int(v));
        case FLAG_LONG:   return disp_make_short((short)disp_get_long(v));
        case FLAG_FLOAT:  return disp_make_short((short)disp_get_float(v));
        case FLAG_DOUBLE: return disp_make_short((short)disp_get_double(v));
        default: ERET(NIL, "cannot convert to short");
    }
}

static disp_val convert_to_int(disp_val v) {
    switch (T(v)) {
        case FLAG_BYTE:   return disp_make_int(disp_get_byte(v));
        case FLAG_SHORT:  return disp_make_int(disp_get_short(v));
        case FLAG_INT:    return v;
        case FLAG_LONG:   return disp_make_int((int)disp_get_long(v));
        case FLAG_FLOAT:  return disp_make_int((int)disp_get_float(v));
        case FLAG_DOUBLE: return disp_make_int((int)disp_get_double(v));
        default: ERET(NIL, "cannot convert to int");
    }
}

static disp_val convert_to_long(disp_val v) {
    switch (T(v)) {
        case FLAG_BYTE:   return disp_make_long(disp_get_byte(v));
        case FLAG_SHORT:  return disp_make_long(disp_get_short(v));
        case FLAG_INT:    return disp_make_long(disp_get_int(v));
        case FLAG_LONG:   return v;
        case FLAG_FLOAT:  return disp_make_long((long)disp_get_float(v));
        case FLAG_DOUBLE: return disp_make_long((long)disp_get_double(v));
        default: ERET(NIL, "cannot convert to long");
    }
}

static disp_val convert_to_float(disp_val v) {
    switch (T(v)) {
        case FLAG_BYTE:   return disp_make_float(disp_get_byte(v));
        case FLAG_SHORT:  return disp_make_float(disp_get_short(v));
        case FLAG_INT:    return disp_make_float(disp_get_int(v));
        case FLAG_LONG:   return disp_make_float(disp_get_long(v));
        case FLAG_FLOAT:  return v;
        case FLAG_DOUBLE: return disp_make_float((float)disp_get_double(v));
        default: ERET(NIL, "cannot convert to float");
    }
}

static disp_val convert_to_double(disp_val v) {
    switch (T(v)) {
        case FLAG_BYTE:   return disp_make_double(disp_get_byte(v));
        case FLAG_SHORT:  return disp_make_double(disp_get_short(v));
        case FLAG_INT:    return disp_make_double(disp_get_int(v));
        case FLAG_LONG:   return disp_make_double(disp_get_long(v));
        case FLAG_FLOAT:  return disp_make_double(disp_get_float(v));
        case FLAG_DOUBLE: return v;
        default: ERET(NIL, "cannot convert to double");
    }
}

// 每个类型定义/强制转换的内置函数，通过宏生成（内联代码，无函数指针类型问题）
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

DEF_TYPE_BUILTIN(byte,   convert_to_byte,   disp_make_byte)
DEF_TYPE_BUILTIN(short,  convert_to_short,  disp_make_short)
DEF_TYPE_BUILTIN(int,    convert_to_int,    disp_make_int)
DEF_TYPE_BUILTIN(long,   convert_to_long,   disp_make_long)
DEF_TYPE_BUILTIN(float,  convert_to_float,  disp_make_float)
DEF_TYPE_BUILTIN(double, convert_to_double, disp_make_double)

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("byte"    , MKB(byte_builtin   , "<#byte>"   ), 1);
    DEF("short"   , MKB(short_builtin  , "<#short>"  ), 1);
    DEF("int"     , MKB(int_builtin    , "<#int>"    ), 1);
    DEF("long"    , MKB(long_builtin   , "<#long>"   ), 1);
    DEF("float"   , MKB(float_builtin  , "<#float>"  ), 1);
    DEF("double"  , MKB(double_builtin , "<#double>" ), 1);
}
