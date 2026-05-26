
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

#if ~NAN_BOXING

union disp_data {
    /* 基本数值类型 */
    char byte_val;
    short short_val;
    int int_val;
    long long_val;
    float float_val;
    double double_val;
};

inline char disp_get_byte(disp_box v) {
    if (T(v) != FLAG_BYTE) ERRO("disp_get_byte failed");
    return v->data->byte_val;
}

inline short disp_get_short(disp_box v) {
    if (T(v) != FLAG_SHORT) ERRO("disp_get_short failed");
    return v->data->short_val;
}

inline int disp_get_int(disp_box v) {
    if (T(v) != FLAG_INT) ERRO("disp_get_int failed");
    return v->data->int_val;
}

inline long disp_get_long(disp_box v) {
    if (T(v) != FLAG_LONG) ERRO("disp_get_long failed");
    return v->data->long_val;
}

inline float disp_get_float(disp_box v) {
    if (T(v) != FLAG_FLOAT) ERRO("disp_get_float failed");
    return v->data->float_val;
}

inline double disp_get_double(disp_box v) {
    if (T(v) != FLAG_DOUBLE) ERRO("disp_get_double failed");
    return v->data->double_val;
}

inline disp_box disp_make_byte(char c) {
    disp_box v = ALLOC(FLAG_BYTE);
    v->data->byte_val = c;
    return v;
}

inline disp_box disp_make_short(short s) {
    disp_box v = ALLOC(FLAG_SHORT);
    v->data->short_val = s;
    return v;
}

inline disp_box disp_make_int(int i) {
    disp_box v = ALLOC(FLAG_INT);
    v->data->int_val = i;
    return v;
}

inline disp_box disp_make_long(long l) {
    disp_box v = ALLOC(FLAG_LONG);
    v->data->long_val = l;
    return v;
}

inline disp_box disp_make_float(float f) {
    disp_box v = ALLOC(FLAG_FLOAT);
    v->data->float_val = f;
    return v;
}

inline disp_box disp_make_double(double d) {
    disp_box v = ALLOC(FLAG_DOUBLE);
    v->data->double_val = d;
    return v;
}

#else // NAN_BOXING

inline char disp_get_byte(disp_box v) {
    if (T(v) != FLAG_BYTE) ERRO("disp_get_byte failed");
    return (char)NAN_UNBOX(v);
}

inline short disp_get_short(disp_box v) {
    if (T(v) != FLAG_SHORT) ERRO("disp_get_short failed");
    return (short)NAN_UNBOX(v);
}

inline int disp_get_int(disp_box v) {
    if (T(v) != FLAG_INT) ERRO("disp_get_int failed");
    return (int)(int32_t)NAN_UNBOX(v);
}

inline long disp_get_long(disp_box v) {
    if (v & NAN_MASK) ERRO("disp_get_long failed");
    return (long)(double)v;
}

inline float disp_get_float(disp_box v) {
    if (T(v) != FLAG_FLOAT) ERRO("disp_get_float failed");
    return (float)(double)NAN_UNBOX(v);
}

inline double disp_get_double(disp_box v) {
    if (v & NAN_MASK) ERRO("disp_get_double failed");
    return (double)v;
}

inline disp_box disp_make_byte(char c) {
    return NAN_BOX(FLAG_BYTE, (disp_box)(unsigned char)c);
}

inline disp_box disp_make_short(short s) {
    return NAN_BOX(FLAG_SHORT, (disp_box)(unsigned short)s);
}

inline disp_box disp_make_int(int i) {
    return NAN_BOX(FLAG_INT, (disp_box)(uint32_t)i);
}

inline disp_box disp_make_long(long l) {
    return disp_make_double((double)l);
}

inline disp_box disp_make_float(float f) {
    return NAN_BOX(FLAG_FLOAT (disp_box)(double)f);
}

inline disp_box disp_make_double(double d) {
    if (d != d) ERRO("can not box NaN");
    return (disp_box)d;
}

#endif // ~NAN_BOXING
