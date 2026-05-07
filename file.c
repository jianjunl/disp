
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

union disp_data {
    /* 文件 */
    struct {
        FILE *file;
        char *mode;
    } file_val;
};

disp_val* disp_make_file(FILE *f, char *mode) {
    disp_val *v = DISP_ALLOC(DISP_FILE);
    v->data->file_val.file = f;
    v->data->file_val.mode = mode;
    return v;
}

FILE* disp_get_file(disp_val *v) {
    if (T(v) != DISP_FILE) {
        ERRO("disp_get_file: not a file object\n");
        return NULL;
    }
    return v->data->file_val.file;
}

char* disp_get_file_mode(disp_val *v) {
    if (T(v) != DISP_FILE) {
        ERRO("disp_get_file: not a file object\n");
        return NULL;
    }
    return v->data->file_val.mode;
}

void disp_set_file(disp_val *v, FILE *f) {
    if (T(v) != DISP_FILE) return;
    GC_ASSIGN_PTR(v->data->file_val.file, f);
}

void disp_set_file_mode(disp_val *v, char *mode) {
    if (T(v) != DISP_FILE) return;
    GC_ASSIGN_PTR(v->data->file_val.mode, mode);
}
