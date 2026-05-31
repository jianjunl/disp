
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

/* ======================== FILE ======================== */

#if DISP_NAN_BOXING
struct disp_data {
    disp_flag_t tag;
#else // DISP_NAN_BOXING
union disp_data {
#endif // DISP_NAN_BOXING
    /* 文件 */
    struct {
        char **argv;
        int argc;
    } args_val;
};

disp_val disp_make_args(int argc, char **argv) {
    disp_val v = ALLOC(FLAG_EXTRA, TAG_ARGS);
    D(v)->args_val.argc = argc;
    D(v)->args_val.argv = argv;
    return v;
}

int disp_get_argc(disp_val v) {
    if (T(v) != TAG_ARGS) {
        ERRO("disp_get_args: not an args object\n");
        return -1;
    }
    return D(v)->args_val.argc;
}

char** disp_get_argv(disp_val v) {
    if (T(v) != TAG_ARGS) {
        ERRO("disp_get_args: not an args object\n");
        return NULL;
    }
    return D(v)->args_val.argv;
}
