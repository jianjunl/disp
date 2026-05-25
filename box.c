
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#ifndef DEBUG
//#define DEBUG
#endif

#include "disp.h"

#if NAN_BOXING

// 基本内联函数
static inline uint64_t disp_tag(disp_box v) { return v >> DISP_TAG_SHIFT; }
static inline uint64_t disp_value(disp_box v) { return v & DISP_VAL_MASK; }
static inline disp_box disp_make_tagged(uint64_t tag, uint64_t val) {
    return (tag << DISP_TAG_SHIFT) | (val & DISP_VAL_MASK);
}

// 指针装箱/拆箱
static inline disp_box disp_box_pointer(void *ptr, uint64_t tag) {
    return disp_make_tagged(tag, (uint64_t)ptr);
}
static inline void* disp_unbox_pointer(disp_box v, uint64_t tag) {
    if (disp_tag(v) != tag) return NULL;
    return (void*)disp_value(v);
}

// 数值构造/读取
static inline disp_box disp_make_byte(char c) {
    return disp_make_tagged(DISP_TAG_BYTE, (uint64_t)(unsigned char)c);
}
static inline char disp_get_byte(disp_box v) {
    return (char)disp_value(v);
}
static inline disp_box disp_make_short(short s) {
    return disp_make_tagged(DISP_TAG_SHORT, (uint64_t)(unsigned short)s);
}
static inline short disp_get_short(disp_box v) {
    return (short)disp_value(v);
}
static inline disp_box disp_make_int(int i) {
    return disp_make_tagged(DISP_TAG_INT, (uint64_t)(uint32_t)i);
}
static inline int disp_get_int(disp_box v) {
    return (int)(int32_t)disp_value(v);
}
static inline disp_box disp_make_long(long l) {
    return disp_make_tagged(DISP_TAG_LONG, (uint64_t)l);
}
static inline long disp_get_long(disp_box v) {
    return (long)disp_value(v);
}

// 浮点数：直接存储原始位，不设标签
static inline disp_box disp_make_float(float f) {
    double d = f;
    disp_box v;
    memcpy(&v, &d, 8);
    return v;
}
static inline float disp_get_float(disp_box v) {
    double d;
    memcpy(&d, &v, 8);
    return (float)d;
}
static inline disp_box disp_make_double(double d) {
    disp_box v;
    memcpy(&v, &d, 8);
    return v;
}
static inline double disp_get_double(disp_box v) {
    double d;
    memcpy(&d, &v, 8);
    return d;
}

#else // NAN_BOXING == 0

#endif // NAN_BOXING
