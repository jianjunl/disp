
#ifndef _NANBOX_H
#define _NANBOX_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

typedef uint16_t disp_flag_t;

// Flags of Primitives (0x7FF0 - 0x7FF7)
#define FLAG_DOUBLE     0x7FF0
#define FLAG_VOID       0x7FF1
#define FLAG_NAN        0x7FF2
#define FLAG_BYTE       0x7FF3
#define FLAG_SHORT      0x7FF4
#define FLAG_INT        0x7FF5
#define FLAG_FLOAT      0x7FF6
#define FLAG_LONG       0x7FF7

// Flags of Pointers (0x7FF8 - 0x7FFF)
#define FLAG_SYMBOL     0x7FF8
#define FLAG_STRING     0x7FF9
#define FLAG_CONS       0x7FFA
#define FLAG_CLOSURE    0x7FFB
#define FLAG_MACRO      0x7FFC
#define FLAG_BUILTIN    0x7FFD
#define FLAG_SYSCALL    0x7FFE
#define FLAG_EXTRA      0x7FFF

// Tags of DATA (Pointers with FLAG_EXTRA)
#define TAG_TYPE        1
#define TAG_FILE        2
#define TAG_CORO        3
#define TAG_CHAN        4
#define TAG_SOCKET      5
#define TAG_THREAD      6
#define TAG_MUTEX       7
#define TAG_COND        8

#define DISP_NAN_BOXING 1

#if DISP_NAN_BOXING

typedef uint64_t disp_val;

typedef struct disp_data disp_data;

typedef union disp_double {
    double d;
    long l;
} disp_double;

typedef struct disp_long {
    long l;
} disp_long;

typedef struct disp_extra {
    disp_flag_t tag;
} disp_extra;

// Flags (High 16 bits)
#define NAN_SHIFT       48
#define NAN_MASK        0x0000FFFFFFFFFFFFULL

#define NAN_FLAG(v) ((disp_flag_t)(v >> NAN_SHIFT))

#define NAN_UNBOX(v) (v & NAN_MASK)

#define NAN_BOX(t, v) ((((disp_val)t) << NAN_SHIFT) | (((disp_val)v) & NAN_MASK))

#define D(v) ((disp_data *)NAN_UNBOX(v))

#define T(v) (NAN_FLAG(v) == FLAG_EXTRA ? ((disp_extra *)D(v))->tag : NAN_FLAG(v))

#define E(v, u)  (v == u)

#define NE(v, u) !E(v, u) 

static inline disp_val V(disp_flag_t f, disp_flag_t t, void *d) {
    if (f == FLAG_EXTRA) ((disp_extra *)d)->tag = t;
    return NAN_BOX(f, d);
}

#define DNULL NAN_BOX(FLAG_VOID, NULL)

#define N(v) (T(v) == FLAG_VOID && !D(v))

#define NN(v) !N(v)

#else // DISP_NAN_BOXING = 0

typedef union disp_data disp_data;

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

#define T(v) (v.flag)

#define E(v, u)  (T(v) == T(u) && D(v) == D(u))

#define NE(v, u) !E(v, u) 

#define V(f, t, d) ((struct disp_val){.flag = (f == FLAG_EXTRA ? t : f), .data = d})

#define DNULL V(FLAG_VOID, 0, NULL)

#define N(v) (T(v) == FLAG_VOID && !D(v))

#define NN(v) !N(v)

#endif // DISP_NAN_BOXING

#endif /* _NANBOX_H */
