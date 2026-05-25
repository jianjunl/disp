
#ifndef _CORE_H
#define _CORE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef DEBUG
//#define DEBUG
#endif
#include "gc/gc.h"
#include "array.h"

#include "box.h"
#include "scope.h"
#include "name.h"
#include "func.h"
#include "closure.h"
#include "prim.h"
#include "cons.h"
#include "number.h"
#include "symbol.h"
#include "eval.h"
#include "io.h"
#include "file.h"
#include "load.h"
#include "info.h"

#include "coro.h"

/* --- disp.c (repl) --- */
void disp_init_globals(void);
void disp_repl(void);

extern disp_box disp_builtin_roots[];

#define NUM_BUILTIN_ROOTS 33

enum disp_builtin_root_idx {
    IDX_NIL = 0, IDX_TRUE, IDX_QUIT,
    IDX_BYTE, IDX_SHORT, IDX_INT, IDX_LONG, IDX_FLOAT, IDX_DOUBLE, IDX_PNTR,
    IDX_LAMBDA, IDX_LET, IDX_LETA, IDX_LETREC, IDX_LETRECA,
    IDX_CONS, IDX_LIST, IDX_QUOTE, IDX_QUASIQUOTE, IDX_UNQUOTE, IDX_UNQUOTE_SPLICING, IDX_APPEND,
    IDX_ELSE, IDX_IF, IDX_BEGIN, IDX_PROGN, IDX_COND, IDX_AND, IDX_OR, IDX_SET, IDX_SETQ, IDX_DEFINE,
    IDX_MODPATH
};

#define NIL              disp_builtin_roots[IDX_NIL]
#define TRUE             disp_builtin_roots[IDX_TRUE]
#define QUIT             disp_builtin_roots[IDX_QUIT]
#define BYTE             disp_builtin_roots[IDX_BYTE]
#define SHORT            disp_builtin_roots[IDX_SHORT]
#define INT              disp_builtin_roots[IDX_INT]
#define LONG             disp_builtin_roots[IDX_LONG]
#define FLOAT            disp_builtin_roots[IDX_FLOAT]
#define LONG             disp_builtin_roots[IDX_LONG]
#define DOUBLE           disp_builtin_roots[IDX_DOUBLE]
#define PNTR             disp_builtin_roots[IDX_PNTR]
#define LAMBDA           disp_builtin_roots[IDX_LAMBDA]
#define LET              disp_builtin_roots[IDX_LET]
#define LETA             disp_builtin_roots[IDX_LETA]
#define LETREC           disp_builtin_roots[IDX_LETREC]
#define LETRECA          disp_builtin_roots[IDX_LETRECA]
#define CONS             disp_builtin_roots[IDX_CONS]
#define LIST             disp_builtin_roots[IDX_LIST]
#define QUOTE            disp_builtin_roots[IDX_QUOTE]
#define QUASIQUOTE       disp_builtin_roots[IDX_QUASIQUOTE]
#define UNQUOTE          disp_builtin_roots[IDX_UNQUOTE]
#define UNQUOTE_SPLICING disp_builtin_roots[IDX_UNQUOTE_SPLICING]
#define APPEND           disp_builtin_roots[IDX_APPEND]
#define ELSE             disp_builtin_roots[IDX_ELSE]
#define IF               disp_builtin_roots[IDX_IF]
#define BEGIN            disp_builtin_roots[IDX_BEGIN]
#define PROGN            disp_builtin_roots[IDX_PROGN]
#define COND             disp_builtin_roots[IDX_COND]
#define AND              disp_builtin_roots[IDX_AND]
#define OR               disp_builtin_roots[IDX_OR]
#define SET              disp_builtin_roots[IDX_SET]
#define SETQ             disp_builtin_roots[IDX_SETQ]
#define DEFINE           disp_builtin_roots[IDX_DEFINE]
#define MODPATH          disp_builtin_roots[IDX_MODPATH]

#include "debug.h"

#endif /* _CORE_H */
