
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
    /* 内置函数 / 系统调用 */
    struct {
        disp_builtin_t func;
        char *desc;
    } builtin; // for special_form/syntax
    struct {
        disp_syscall_t func;
        char *desc;
    } syscall; // for function/primitive
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, builtin.func),
    GC_OFF(disp_data, builtin.desc)
);

disp_builtin_t disp_get_builtin(disp_val *v) {
    if (v->flag == DISP_BUILTIN) {
        return v->data->builtin.func;
    }
    ERET(NULL, "disp_get_builtin failed: %s\n", disp_str(v));
}

char* disp_get_builtin_desc(disp_val *v) {
    if (v->flag == DISP_BUILTIN) {
        return v->data->builtin.desc;
    }
    ERET(NULL, "disp_get_builtin_desc failed: %s\n", strerror (errno));
}

disp_syscall_t disp_get_syscall(disp_val *v) {
    if (v->flag == DISP_SYSCALL) {
        return v->data->syscall.func;
    }
    ERET(NULL, "disp_get_syscall failed: %s\n", strerror (errno));
}

char* disp_get_syscall_desc(disp_val *v) {
    if (v->flag == DISP_SYSCALL) {
        return v->data->syscall.desc;
    }
    ERET(NULL, "disp_get_syscall_desc failed: %s\n", strerror (errno));
}

disp_val* disp_make_builtin(disp_builtin_t f, char *d) {
    disp_val *v = DISP_ALLOC_TI(DISP_BUILTIN);
    v->data->builtin.func = f;
    v->data->builtin.desc = d;
    return v;
}

disp_val* disp_make_syscall(disp_syscall_t f, char *d) {
    disp_val *v = DISP_ALLOC_TI(DISP_SYSCALL);
    v->data->syscall.func = f;
    v->data->syscall.desc = d;
    return v;
}
