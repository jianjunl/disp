
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

struct disp_data {
#if DISP_NAN_BOXING
    disp_flag_t tag;
#endif // DISP_NAN_BOXING
    char **argv;
    int argc;
};

disp_val disp_make_args(int argc, char **argv) {
    disp_val v = ALLOC(FLAG_EXTRA, TAG_ARGS);
    D(v)->argc = argc;
    D(v)->argv = argv;
    return v;
}

int disp_get_argc(disp_val v) {
    if (T(v) != TAG_ARGS) {
        ERRO("disp_get_args: not an args object\n");
        return -1;
    }
    return D(v)->argc;
}

char** disp_get_argv(disp_val v) {
    if (T(v) != TAG_ARGS) {
        ERRO("disp_get_args: not an args object\n");
        return NULL;
    }
    return D(v)->argv;
}
