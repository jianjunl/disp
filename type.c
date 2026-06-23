
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <ucontext.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

inline disp_val disp_convert_to_byte(disp_val v) {
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

inline disp_val disp_convert_to_short(disp_val v) {
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

inline disp_val disp_convert_to_int(disp_val v) {
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

inline disp_val disp_convert_to_long(disp_val v) {
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

inline disp_val disp_convert_to_float(disp_val v) {
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

inline disp_val disp_convert_to_double(disp_val v) {
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

#if DISP_BOXING

inline disp_env_t* disp_get_type_env(disp_val v) {
    if (N(v) || E(v, NIL)) return NULL;
    if (T(v) != FLAG_TYPE) {
	ERRO("disp_get_type_env failed");
        return NULL;
    }
    return (disp_env_t *)BOX_UNBOX(v);
}

inline disp_val disp_new_type(disp_val parent) {
    if (E(parent, NIL)) return  GSYM(PROTO);
    disp_env_t *e = N(parent) ? NULL : disp_get_type_env(parent);
    e = disp_new_env(e);
    return BOX_BOX(FLAG_TYPE, e);
}

#else // DISP_BOXING

inline disp_env_t* disp_get_type_env(disp_val v) {
    if (N(v) || E(v, NIL)) return NULL;
    if (T(v) != FLAG_TYPE) {
	ERRO("disp_get_type_env failed");
        return NULL;
    }
    return (disp_env_t *)v.x;
}

inline disp_val disp_new_type(disp_val parent) {
    if (E(parent, NIL)) return  GSYM(PROTO);
    disp_env_t *e = N(parent) ? NULL : disp_get_type_env(parent);
    e = disp_new_env(e);
    return (disp_val){.flag = FLAG_TYPE, .x = (uint64_t)e};
}

#endif // DISP_BOXING
