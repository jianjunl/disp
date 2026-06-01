
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <ucontext.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

/* ======================== Funcs ======================== */

struct disp_data {
    /* 类型 */
#if DISP_NAN_BOXING
    disp_flag_t tag;
#endif // DISP_NAN_BOXING
    char *name;
    disp_val decl;
};

GC_STRUCT_TI(disp_data,
    GC_OFF(disp_data, name),
    GC_OFF(disp_data, decl)
);

char* disp_get_type_name(disp_val v) {
    if (T(v) != TAG_TYPE) {
	ERRO("disp_get_type_name failed: %s\n", strerror (errno));
    }
    return D(v)->name;
}

disp_val disp_get_type_decl(disp_val v) {
    if (T(v) != TAG_TYPE) {
	ERRO("T_decl failed: %s\n", strerror (errno));
    }
    return D(v)->decl;
}

disp_val disp_define_type(char *name, disp_val decl) {
    disp_val v = ALLOC_TI(FLAG_EXTRA, TAG_TYPE);
    D(v)->name = name;
    D(v)->decl = decl;
    return disp_define_symbol_by_name(NULL, name, v, 1);
}
