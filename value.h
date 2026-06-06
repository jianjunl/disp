
#ifndef _NANBOX_H
#define _NANBOX_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#define DISP_BOXING     0  // 2 = NaN-Addr-Boxing, 1 = Addr-Boxing, 0 = none
#define DISP_NAN_BOXING (DISP_BOXING >> 1)

typedef struct disp_data disp_data;

#if DISP_NAN_BOXING

typedef uint16_t disp_flag_t;

#define FLAG_DOUBLE     0

// Tags of DATA (Pointers with FLAG_EXTRA)
#define TAG_ARGS        1
#define TAG_TYPE        2
#define TAG_LONG        3
#define TAG_FILE        4
#define TAG_CORO        5
#define TAG_CHAN        6
#define TAG_SOCKET      7
#define TAG_THREAD      8
#define TAG_MUTEX       9
#define TAG_COND        10

// Flags of Primitives (0xFFFF - 0xFFF8)
#define FLAG_VOID       0xFFFF
#define FLAG_NAN        0xFFFE
#define FLAG_BYTE       0xFFFD
#define FLAG_SHORT      0xFFFC
#define FLAG_INT        0xFFFB
#define FLAG_FLOAT      0xFFFA
#define FLAG_LONG       0xFFF9

// Flags of Pointers (0x7FFF - 0x7FF8)
#define FLAG_SYMBOL     0x7FFF
#define FLAG_STRING     0x7FFE
#define FLAG_CONS       0x7FFD
#define FLAG_CLOSURE    0x7FFC
#define FLAG_MACRO      0x7FFB
#define FLAG_BUILTIN    0x7FFA
#define FLAG_SYSCALL    0x7FF9

#define FLAG_EXTRA      0x7FF8

typedef uint64_t disp_val;

typedef union disp_long {
    long l;
} disp_long;

typedef struct disp_extra {
    disp_flag_t tag;
} disp_extra;

// Flags (High 16 bits)
#define BOX_SHIFT       48
#define TAG_MASK        0xFFFF
#define BOX_MASK        0x0000FFFFFFFFFFFFULL
#define NaN_HEADER      0x7FF8000000000000ULL

#define FLAG(v) ((disp_flag_t)(v >> BOX_SHIFT))
#define SIGN(v) (FLAG(v) & 0x8000)
#define BOX_FLAG(v) (((disp_flag_t)((disp_flag_t)(v >> BOX_SHIFT))))

#define BOX_UNBOX(v) (v & BOX_MASK)

#define BOX_BOX(t, v) (NaN_HEADER | ((((disp_val)t) & TAG_MASK) << BOX_SHIFT) | (((disp_val)v) & BOX_MASK))

#define D(v) ((disp_data *)BOX_UNBOX(v))

static inline disp_flag_t T(disp_val v) {
    uint64_t exp = (v >> 52) & 0x7FF;
    if (exp != 0x7FF)        // 不是 NaN，则是 double
        return FLAG_DOUBLE;
    // 是 NaN，从高 16 位提取 tag
    disp_flag_t f = (v >> 48) & 0xFFFF;
    if (f == FLAG_EXTRA) {
        disp_extra *e = (disp_extra *)(v & BOX_MASK);
        return e->tag;
    }
    return f;
}

#define E(v, u)  (v == u)

#define NE(v, u) !E(v, u) 

static inline disp_val V(disp_flag_t f, disp_flag_t t, void *d) {
    if (f == FLAG_EXTRA) ((disp_extra *)d)->tag = t;
    return BOX_BOX(f, d);
}

#define DNULL BOX_BOX(FLAG_VOID, NULL)

#define N(v) (T(v) == FLAG_VOID && !D(v))

#define NN(v) !N(v)

#else // DISP_NAN_BOXING = 0

typedef uint8_t disp_flag_t;

#define FLAG_DOUBLE     UINT8_C(0x71)

// Tags of DATA (Pointers with FLAG_EXTRA)
#define TAG_ARGS        UINT8_C(1)
#define TAG_TYPE        UINT8_C(2)
#define TAG_LONG        UINT8_C(3)
#define TAG_FILE        UINT8_C(4)
#define TAG_CORO        UINT8_C(5)
#define TAG_CHAN        UINT8_C(6)
#define TAG_SOCKET      UINT8_C(7)
#define TAG_THREAD      UINT8_C(8)
#define TAG_MUTEX       UINT8_C(9)
#define TAG_COND        UINT8_C(10)

// Flags of Primitives (0x7FFF - 0x7FF8)
#define FLAG_VOID       UINT8_C(0xFF)
#define FLAG_NAN        UINT8_C(0xFE)
#define FLAG_BYTE       UINT8_C(0xFD)
#define FLAG_SHORT      UINT8_C(0xFC)
#define FLAG_INT        UINT8_C(0xFB)
#define FLAG_FLOAT      UINT8_C(0xFA)
#define FLAG_LONG       UINT8_C(0x78)

// Flags of Pointers (0xFFFF - 0xFFF8)
#define FLAG_SYMBOL     UINT8_C(0x7F)
#define FLAG_STRING     UINT8_C(0x7E)
#define FLAG_CONS       UINT8_C(0x7D)
#define FLAG_CLOSURE    UINT8_C(0x7C)
#define FLAG_MACRO      UINT8_C(0x7B)
#define FLAG_BUILTIN    UINT8_C(0x7A)
#define FLAG_SYSCALL    UINT8_C(0x79)

#define FLAG_EXTRA      UINT8_C(0x78)

#if DISP_BOXING

typedef union disp_long {
    double d;
    long l;
} disp_long;

typedef uint64_t disp_val;

// Flags (High 8 bits)
#define BOX_SHIFT       56
#define BOX_MASK        0x00FFFFFFFFFFFFFFULL

#define BOX_FLAG(v) ((disp_flag_t)(v >> BOX_SHIFT))

#define BOX_UNBOX(v) (v & BOX_MASK)

#define BOX_BOX(t, v) ((((disp_val)(t & 0xFF)) << BOX_SHIFT) | (((disp_val)v) & BOX_MASK))

#define D(v) ((disp_data *)BOX_UNBOX(v))

#define T(v) BOX_FLAG(v)

#define E(v, u)  (v == u)

#define NE(v, u) !E(v, u) 

#define V(f, t, d) BOX_BOX(f == FLAG_EXTRA ? t : f, d)

#define DNULL BOX_BOX(FLAG_VOID, NULL)

#define N(v) (T(v) == FLAG_VOID && !D(v))

#define NN(v) !N(v)

#else // DISP_BOXING

typedef struct disp_val {
    union {
        disp_data *data;
        /* 基本数值类型 */
        char byte_val;
        short short_val;
        int int_val;
        long long_val;
        float float_val;
        double double_val;
    };
    disp_flag_t flag;
} disp_val ;

#define D(v) v.data

#define T(v) v.flag

#define E(v, u)  (T(v) == T(u) && D(v) == D(u))

#define NE(v, u) !E(v, u) 

#define V(f, t, d) ((struct disp_val){.flag = (f == FLAG_EXTRA ? t : f), .data = d})

#define DNULL V(FLAG_VOID, 0, NULL)

#define N(v) (T(v) == FLAG_VOID && !D(v))

#define NN(v) !N(v)

#endif // DISP_BOXING

#endif // DISP_NAN_BOXING

#endif /* _NANBOX_H */
