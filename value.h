
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
typedef union disp_data disp_data;

// Flags of Primitives (0xFFF8 - 0xFFFF)
#define FLAG_VOID       0xFFF8
#define FLAG_NAN        0xFFF9
#define FLAG_BYTE       0xFFFA
#define FLAG_SHORT      0xFFFB
#define FLAG_INT        0xFFFC
#define FLAG_LONG       0xFFFD
#define FLAG_FLOAT      0xFFFE
#define FLAG_DOUBLE     0xFFFF

// Flags of Pointers (0x7FF8 - 0x7FFF)
#define FLAG_SYMBOL     0x7FF8
#define FLAG_STRING     0x7FF9
#define FLAG_CONS       0x7FFA
#define FLAG_CLOSURE    0x7FFB
#define FLAG_MACRO      0x7FFC
#define FLAG_BUILTIN    0x7FFD
#define FLAG_SYSCALL    0x7FFE
#define FLAG_EXTRA      0x7FFF

// Tags of DATA (FLAG_EXTRA of Pointers)
#define TAG_TYPE        1
#define TAG_FILE        2
#define TAG_CORO        3
#define TAG_CHAN        4
#define TAG_SOCKET      5
#define TAG_THREAD      6
#define TAG_MUTEX       7
#define TAG_COND        8

#define NAN_MASK        0xFFF8000000000000ULL

#define DISP_NAN_BOXING      0

#if DISP_NAN_BOXING

typedef uint64_t disp_val;

// Flags (High 16 bits)
#define TAG_SHIFT       48
#define TAG_MASK        0x0000FFFFFFFFFFFFULL

#define NAN_FLAG(v) ((disp_flag_t)(v >> TAG_SHIFT))

#define NAN_UNBOX(v) (v & TAG_MASK)

#define NAN_BOX(t, v) ((((disp_val)t) << TAG_SHIFT) | (v & TAG_MASK))

#define D(v) (disp_data*)NAN_UNBOX(v);

#define T(v) (NAN_FLAG(v) == FLAG_EXTRA ? D(v)->tag : NAN_FLAG(v))

#define E(v, u)  (v == u)

#define NE(v, u) !E(v, u) 

#define V(f, t, d) NAN_BOX(f, d)

#define DNULL V(FLAG_VOID, 0, NULL)

#define N(v) (D(v) == NULL) 

#define NN(v) !N(v)

#else // DISP_NAN_BOXING = 0

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

#define N(v) (D(v) == NULL) 

#define NN(v) !N(v)

#endif // DISP_NAN_BOXING

#endif /* _NANBOX_H */
