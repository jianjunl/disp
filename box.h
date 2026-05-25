#ifndef BOX_H
#define BOX_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

typedef uint16_t disp_flag_t;
typedef union disp_data disp_data;

#define FLAG_TRUE       0xFFF8
#define FLAG_FALSE      0xFFF9
#define FLAG_BYTE       0xFFFA
#define FLAG_SHORT      0xFFFB
#define FLAG_INT        0xFFFC
#define FLAG_LONG       0xFFFD
#define FLAG_FLOAT      0xFFFE
#define FLAG_DOUBLE     0xFFFF

// 指针标签（0x7F80 - 0x7FFF）
#define FLAG_SYMBOL     0x7FF8
#define FLAG_STRING     0x7FF9
#define FLAG_CONS       0x7FFA
#define FLAG_CLOSURE    0x7FFB
#define FLAG_MACRO      0x7FFC
#define FLAG_BUILTIN    0x7FFD
#define FLAG_SYSCALL    0x7FFE
#define FLAG_EXTRA      0x7FFF

#define TAG_TYPE       0
#define TAG_FILE       1
#define TAG_CORO       2
#define TAG_CHAN       3
#define TAG_SOCKET     4
#define TAG_THREAD     5
#define TAG_MUTEX      6
#define TAG_COND       7

#define NAN_BOXING 0

#if NAN_BOXING

typedef uint64_t disp_box;

// 标签定义（高16位）
#define TAG_SHIFT 48
#define TAG_MASK  0x0000FFFFFFFFFFFFULL

// 基本内联函数
static inline disp_flag_t _flag(disp_box v) {
    return (disp_flag_t)(v >> TAG_SHIFT);
}
static inline disp_box _unbox(disp_box v) {
    return v & TAG_MASK;
}
static inline disp_box _box(disp_flag_t t, disp_box v) {
    disp_box tag = (disp_box)t;
    return (tag << TAG_SHIFT) | (v & TAG_MASK);
}

#define T(v) (_flag(v) == FLAG_EXTRA ? ((disp_data*)_unbox(v))->tag : flag(v))
// 指针装箱/拆箱
static inline disp_box _pack(disp_flag_t t, void *ptr) {
    return _box(t, (disp_box)ptr);
}
static inline void* disp_unbox_pointer(disp_box v, uint64_t tag) {
    if (disp_tag(v) != tag) return NULL;
    return (void*)disp_value(v);
}

// 数值构造/读取
static inline disp_box disp_make_byte(char c) {
    return disp_make_tagged(FLAG_BYTE, (uint64_t)(unsigned char)c);
}
static inline char disp_get_byte(disp_box v) {
    return (char)disp_value(v);
}
static inline disp_box disp_make_short(short s) {
    return disp_make_tagged(FLAG_SHORT, (uint64_t)(unsigned short)s);
}
static inline short disp_get_short(disp_box v) {
    return (short)disp_value(v);
}
static inline disp_box disp_make_int(int i) {
    return disp_make_tagged(FLAG_INT, (uint64_t)(uint32_t)i);
}
static inline int disp_get_int(disp_box v) {
    return (int)(int32_t)disp_value(v);
}
static inline disp_box disp_make_long(long l) {
    return disp_make_tagged(FLAG_LONG, (uint64_t)l);
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

#else // NAN_BOXING = 0

typedef struct disp_val {
    disp_flag_t flag;
    disp_data *data;
} disp_val ;
typedef disp_val* disp_box;

#define T(v) v->flag

#endif // NAN_BOXING

#endif /* BOX_H */
