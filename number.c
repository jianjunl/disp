
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

#if DISP_BOXING

inline char disp_get_byte(disp_val v) {
    if (T(v) != FLAG_BYTE) ERRO("disp_get_byte failed");
    return (char)(int8_t)(BOX_UNBOX(v) & 0xFF);
}

inline short disp_get_short(disp_val v) {
    if (T(v) != FLAG_SHORT) ERRO("disp_get_short failed");
    return (short)(int16_t)(BOX_UNBOX(v) & 0xFFFF);
}

inline int disp_get_int(disp_val v) {
    if (T(v) != FLAG_INT) ERRO("disp_get_int failed");
    return (int)(int32_t)BOX_UNBOX(v);
}

#if DISP_NAN_BOXING

inline long disp_get_long(disp_val v) {
    disp_flag_t t = T(v);
    if (t == FLAG_LONG) {
        // 直接装箱：48 位符号扩展
        uint64_t payload = BOX_UNBOX(v) & 0xFFFFFFFFFFFFULL;
        if (payload & (1ULL << 47)) {
            payload |= ~((1ULL << 48) - 1);
        }
        return (long)payload;
    } else if (t == TAG_LONG) {
        // 堆分配
        disp_long *p = (disp_long *)D(v);
        return p->l;
    } else {
        ERRO("disp_get_long: not a long value");
        return 0;
    }
}

#else // DISP_NAN_BOXING

inline long disp_get_long(disp_val v) {
    if (T(v) != FLAG_LONG) ERRO("disp_get_long failed");
    disp_long *p = (disp_long*)BOX_UNBOX(v);
    return p->l;
}

#endif // DISP_NAN_BOXING

inline float disp_get_float(disp_val v) {
    if (T(v) != FLAG_FLOAT) ERRO("disp_get_float failed");
    uint32_t bits = (uint32_t)BOX_UNBOX(v);
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

#if DISP_NAN_BOXING

inline double disp_get_double(disp_val v) {
    if (T(v) != FLAG_DOUBLE) ERRO("disp_get_double failed");
    double d;
    memcpy(&d, &v, sizeof(d));
    return d;
}

#else // DISP_NAN_BOXING

inline double disp_get_double(disp_val v) {
    if (T(v) != FLAG_DOUBLE) ERRO("disp_get_double failed");
    disp_long *p = (disp_long*)BOX_UNBOX(v);
    return p->d;
}

#endif // DISP_NAN_BOXING

inline disp_val disp_make_byte(char c) {
    return BOX_BOX(FLAG_BYTE, (uint8_t)c);
}

inline disp_val disp_make_short(short s) {
    return BOX_BOX(FLAG_SHORT, (uint16_t)s);
}

inline disp_val disp_make_int(int i) {
    return BOX_BOX(FLAG_INT, (uint32_t)i);
}

#if DISP_NAN_BOXING

inline disp_val disp_make_long(long l) {
    // 48 位有符号范围： -2^47  ~  2^47-1
    const long LONG_48_MIN = -(1LL << 47);
    const long LONG_48_MAX = (1LL << 47) - 1;

    if (l >= LONG_48_MIN && l <= LONG_48_MAX) {
        // 直接装箱：保留低 48 位（符号自动通过截断保留）
        uint64_t payload = (uint64_t)l & 0xFFFFFFFFFFFFULL;
        return BOX_BOX(FLAG_LONG, payload);
    } else {
        // 堆分配
        disp_long *p = (disp_long *)gc_malloc(sizeof(disp_long));
        p->l = l;
        return V(FLAG_EXTRA, TAG_LONG, p);
    }
}

#else // DISP_NAN_BOXING

inline disp_val disp_make_long(long l) {
    disp_long *p = (disp_long *)gc_malloc(sizeof(disp_long));
    p->l = l;
    return BOX_BOX(FLAG_LONG, p);
}

#endif // DISP_NAN_BOXING

inline disp_val disp_make_float(float f) {
    if (f != f) return NaN;
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return BOX_BOX(FLAG_FLOAT, bits);
}

#if DISP_NAN_BOXING

inline disp_val disp_make_double(double d) {
    if (d != d) return NaN;
    disp_val v;
    memcpy(&v, &d, sizeof(v));
    return v;
}

#else // DISP_NAN_BOXING

inline disp_val disp_make_double(double d) {
    disp_long *p = (disp_long *)gc_malloc(sizeof(disp_long));
    p->d = d;
    return BOX_BOX(FLAG_DOUBLE, p);
}

#endif // DISP_NAN_BOXING

#else // DISP_BOXING

inline char disp_get_byte(disp_val v) {
    if (T(v) != FLAG_BYTE) ERRO("disp_get_byte failed");
    return v.byte_val;
}

inline short disp_get_short(disp_val v) {
    if (T(v) != FLAG_SHORT) ERRO("disp_get_short failed");
    return v.short_val;
}

inline int disp_get_int(disp_val v) {
    if (T(v) != FLAG_INT) ERRO("disp_get_int failed");
    return v.int_val;
}

inline long disp_get_long(disp_val v) {
    if (T(v) != FLAG_LONG) ERRO("disp_get_long failed");
    return v.long_val;
}

inline float disp_get_float(disp_val v) {
    if (T(v) != FLAG_FLOAT) ERRO("disp_get_float failed");
    return v.float_val;
}

inline double disp_get_double(disp_val v) {
    if (T(v) != FLAG_DOUBLE) ERRO("disp_get_double failed");
    return v.double_val;
}

inline disp_val disp_make_byte(char c) {
    return (disp_val){.flag = FLAG_BYTE, .byte_val = c};
}

inline disp_val disp_make_short(short s) {
    return (disp_val){.flag = FLAG_SHORT, .short_val = s};
}

inline disp_val disp_make_int(int i) {
    return (disp_val){.flag = FLAG_INT, .int_val = i};
}

inline disp_val disp_make_long(long l) {
    return (disp_val){.flag = FLAG_LONG, .long_val = l};
}

inline disp_val disp_make_float(float f) {
    return (disp_val){.flag = FLAG_FLOAT, .float_val = f};
}

inline disp_val disp_make_double(double d) {
    return (disp_val){.flag = FLAG_DOUBLE, .double_val = d};
}

#endif // DISP_BOXING
