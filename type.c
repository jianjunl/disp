
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

union disp_data {
    /* 类型 */
    struct {
        char *name;
        disp_val decl;
    } type_val;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, type_val.name),
    GC_OFF(disp_data, type_val.decl)
);

char* disp_get_type_name(disp_val v) {
    if (T(v) != TAG_TYPE) {
	ERRO("disp_get_type_name failed: %s\n", strerror (errno));
    }
    return D(v)->type_val.name;
}

disp_val disp_get_type_decl(disp_val v) {
    if (T(v) != TAG_TYPE) {
	ERRO("T_decl failed: %s\n", strerror (errno));
    }
    return D(v)->type_val.decl;
}

disp_val disp_define_type(char *name, disp_val decl) {
    disp_val v = ALLOC_TI(FLAG_EXTRA, TAG_TYPE);
    D(v)->type_val.name = name;
    D(v)->type_val.decl = decl;
    return disp_define_symbol(NULL, name, v, 1);
}
