#ifndef BOX_H
#define BOX_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

/* Value types */
typedef enum {
    DISP_VOID,
    DISP_BYTE, DISP_SHORT, DISP_INT, DISP_LONG, DISP_FLOAT, DISP_DOUBLE, DISP_STRING, DISP_CONS,
    DISP_SYMBOL,
    DISP_SYSCALL, DISP_BUILTIN, DISP_CLOSURE, DISP_MACRO,
    DISP_FILE,
    DISP_CORO, DISP_CHAN, DISP_SOCKET,
    DISP_THREAD, DISP_MUTEX, DISP_COND,
    DISP_TYPE
} disp_flag_t;

/* Opaque types */

typedef union disp_data disp_data;

typedef struct disp_val {
    disp_flag_t flag;
    disp_data *data;
} disp_val ;

typedef disp_val* disp_box;

#endif /* BOX_H */
