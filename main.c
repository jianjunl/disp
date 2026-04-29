
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
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

/* ======================== MAIN ======================== */

int main(int argc, char **argv) {

    disp_init_globals();

    // 为 REPL 压入伪源文件
    disp_push_source("<stdin>");

    disp_load(argc > 1 ? argv[1] : "init.disp");

    disp_repl();

    return 0;

    disp_pop_source();  // 清理
}
