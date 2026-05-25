#ifndef BOX_H
#define BOX_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#define DISP_NAN_BOXING 0

#if DISP_NAN_BOXING


#else // DISP_NAN_BOXING = 0

// Opaque types
typedef union disp_data disp_data;
typedef struct disp_val {
    int flag;
    disp_data *data;
} disp_val ;
typedef disp_val* disp_box;

#endif // DISP_NAN_BOXING

// Value box tags

#define DISP_VOID        0
#define DISP_BYTE        1
#define DISP_SHORT       2
#define DISP_INT         3
#define DISP_LONG        4
#define DISP_FLOAT       5
#define DISP_DOUBLE      6

#define DISP_STRING      7
#define DISP_CONS        8 
#define DISP_SYMBOL      9
#define DISP_SYSCALL     10
#define DISP_BUILTIN     11
#define DISP_CLOSURE     12
#define DISP_MACRO       13
#define DISP_FILE        14
#define DISP_CORO        15
#define DISP_CHAN        16
#define DISP_SOCKET      17
#define DISP_THREAD      18
#define DISP_MUTEX       19
#define DISP_COND        20
#define DISP_TYPE        21

#endif /* BOX_H */
