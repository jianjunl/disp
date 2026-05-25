#ifndef BOX_H
#define BOX_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#define NAN_BOXING 0

#if NAN_BOXING


#else // NAN_BOXING = 0

// Opaque types
typedef union disp_data disp_data;
typedef struct disp_val {
    int flag;
    disp_data *data;
} disp_val ;
typedef disp_val* disp_box;

#endif // NAN_BOXING

// Value box tags

#define FLAG_VOID        0
#define FLAG_BYTE        1
#define FLAG_SHORT       2
#define FLAG_INT         3
#define FLAG_LONG        4
#define FLAG_FLOAT       5
#define FLAG_DOUBLE      6

#define FLAG_STRING      7
#define FLAG_CONS        8 
#define FLAG_SYMBOL      9
#define FLAG_SYSCALL     10
#define FLAG_BUILTIN     11
#define FLAG_CLOSURE     12
#define FLAG_MACRO       13
#define FLAG_FILE        14
#define FLAG_CORO        15
#define FLAG_CHAN        16
#define FLAG_SOCKET      17
#define FLAG_THREAD      18
#define FLAG_MUTEX       19
#define FLAG_COND        20
#define FLAG_TYPE        21

#endif /* BOX_H */
