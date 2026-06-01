
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

void disp_init_env(void);

#include "value.h"
#include "args.h"
#include "env.h"
#include "name.h"
#include "func.h"
#include "closure.h"
#include "number.h"
#include "cons.h"
#include "literal.h"
#include "symbol.h"
#include "eval.h"
#include "io.h"
#include "file.h"
#include "load.h"
#include "info.h"

#include "coro.h"

/* --- disp.c (repl) --- */
void disp_init(void);
void disp_repl(void);

#include "global.h"
#include "debug.h"

#endif /* _CORE_H */
