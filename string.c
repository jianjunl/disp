
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

union disp_data {
    struct {
        char *str;
        size_t len;
    } string_val;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, string_val.str)
);

char* disp_get_str(disp_box v) {
    if (v->flag != DISP_STRING) {
	ERRO("disp_get_string failed: %s\n", strerror (errno));
    }
    return v->data->string_val.str;
}

size_t disp_get_str_len(disp_box v) {
    if (v->flag != DISP_STRING) {
	ERRO("disp_get_string failed: %s\n", strerror (errno));
    }
    return v->data->string_val.len;
}

disp_box disp_make_string(const char *s) {
    disp_box v = DISP_ALLOC_TI(DISP_STRING);
    v->data->string_val.str = gc_strdup(s);
    v->data->string_val.len = strlen(s);
    return v;
}
