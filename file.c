
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
    /* 文件 */
#if DISP_NAN_BOXING
    disp_flag_t tag;
#endif // DISP_NAN_BOXING
    FILE *file;
    char *mode;
};

GC_STRUCT_TI(disp_data,
    GC_OFF(disp_data, mode)
);

disp_val disp_make_file(FILE *f, char *mode) {
    disp_val v = ALLOC_TI(FLAG_EXTRA, TAG_FILE);
    D(v)->file = f;
    D(v)->mode = mode;
    return v;
}

FILE* disp_get_file(disp_val v) {
    if (T(v) != TAG_FILE) {
        ERRO("disp_get_file: not a file object\n");
        return NULL;
    }
    return D(v)->file;
}

char* disp_get_file_mode(disp_val v) {
    if (T(v) != TAG_FILE) {
        ERRO("disp_get_file: not a file object\n");
        return NULL;
    }
    return D(v)->mode;
}

void disp_set_file(disp_val v, FILE *f) {
    if (T(v) != TAG_FILE) return;
    GC_ASSIGN_PTR(D(v)->file, f);
}

void disp_set_file_mode(disp_val v, char *mode) {
    if (T(v) != TAG_FILE) return;
    GC_ASSIGN_PTR(D(v)->mode, mode);
}
