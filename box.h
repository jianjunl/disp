#ifndef BOX_H
#define BOX_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

// 特殊常量标签
#define FLAG_VOID       0x7FF0ULL

// 数值标签（整数类）
#define FLAG_BYTE       0x7FF1ULL
#define FLAG_SHORT      0x7FF2ULL
#define FLAG_INT        0x7FF3ULL
#define FLAG_LONG       0x7FF4ULL
#define FLAG_LONG_LONG  0x7FF5ULL
#define FLAG_FLOAT      0x7FF6ULL
#define FLAG_DOUBLE     0x7FF7ULL

// 指针标签（0x7FF8 - 0x7FFF）
#define FLAG_STRING     0x7FF8ULL
#define FLAG_CONS       0x7FF9ULL
#define FLAG_SYMBOL     0x7FFAULL
#define FLAG_CLOSURE    0x7FFBULL
#define FLAG_MACRO      0x7FFCULL
#define FLAG_BUILTIN    0x7FFDULL
#define FLAG_SYSCALL    0x7FFEULL
#define FLAG_EXTRA      0x7FFFULL

#define FLAG_TYPE       0ULL
#define FLAG_FILE       1ULL
#define FLAG_CORO       2ULL
#define FLAG_CHAN       3ULL
#define FLAG_SOCKET     4ULL
#define FLAG_THREAD     5ULL
#define FLAG_MUTEX      6ULL
#define FLAG_COND       7ULL

#define NAN_BOXING 0

#if NAN_BOXING

typedef uint64_t disp_box;

// 标签定义（高16位）
#define FLAG_SHIFT 48
#define FLAG_MASK  0xFFFF000000000000ULL
#define DISP_VAL_MASK  0x0000FFFFFFFFFFFFULL

// 基本内联函数
static inline uint64_t disp_tag(disp_box v) { return v >> FLAG_SHIFT; }
static inline uint64_t disp_value(disp_box v) { return v & DISP_VAL_MASK; }
static inline disp_box disp_make_tagged(uint64_t tag, uint64_t val) {
    return (tag << FLAG_SHIFT) | (val & DISP_VAL_MASK);
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

// Opaque types
typedef union disp_data disp_data;
typedef struct disp_val {
    uint16_t flag;
    disp_data *data;
} disp_val ;
typedef disp_val* disp_box;

#endif // NAN_BOXING

#endif /* BOX_H */
