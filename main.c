
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <assert.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

/* ======================== MAIN ======================== */

extern void disp_push_source(const char *filename);
extern void disp_pop_source(void);

int main(int argc, char **argv) {

    disp_init();
    DEF(":args", disp_make_args(argc, argv), 1);
    ARGS = disp_find_symbol(NULL, ":args");

// 48 位范围内：直接装箱
disp_val v1 = disp_make_long(12345);
assert(T(v1) == FLAG_LONG);
assert(disp_get_long(v1) == 12345);

// 超出范围：堆分配
disp_val v2 = disp_make_long(1LL << 50);
assert(T(v2) == TAG_LONG);
assert(disp_get_long(v2) == (1LL << 50));

    // 为 REPL 压入伪源文件
    disp_push_source("<stdin>");

    disp_import("repl.lisp");
    //disp_repl();

    return 0;

    disp_pop_source();  // 清理
}
