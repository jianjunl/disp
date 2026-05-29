
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
        FILE *file;
        char *mode;
    } file_val;
};

#if DISP_NAN_BOXING
GC_STRUCT_TI(disp_data,
#else // DISP_NAN_BOXING
GC_UNION_TI(disp_data,
#endif // DISP_NAN_BOXING
    GC_OFF(disp_data, file_val.mode)
);

disp_val disp_make_file(FILE *f, char *mode) {
    disp_val v = ALLOC_TI(FLAG_EXTRA, TAG_FILE);
    D(v)->file_val.file = f;
    D(v)->file_val.mode = mode;
    return v;
}

FILE* disp_get_file(disp_val v) {
    if (T(v) != TAG_FILE) {
        ERRO("disp_get_file: not a file object\n");
        return NULL;
    }
    return D(v)->file_val.file;
}

char* disp_get_file_mode(disp_val v) {
    if (T(v) != TAG_FILE) {
        ERRO("disp_get_file: not a file object\n");
        return NULL;
    }
    return D(v)->file_val.mode;
}

void disp_set_file(disp_val v, FILE *f) {
    if (T(v) != TAG_FILE) return;
    GC_ASSIGN_PTR(D(v)->file_val.file, f);
}

void disp_set_file_mode(disp_val v, char *mode) {
    if (T(v) != TAG_FILE) return;
    GC_ASSIGN_PTR(D(v)->file_val.mode, mode);
}
