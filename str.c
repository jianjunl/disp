
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

struct disp_data {
    struct {
        char *str;
        size_t len;
    } string_val;
};

GC_STRUCT_TI(disp_data,
    GC_OFF(disp_data, string_val.str)
);

char* disp_get_str(disp_val v) {
    if (T(v) != FLAG_STRING) {
	ERRO("disp_get_string failed: %s\n", strerror (errno));
    }
    return D(v)->string_val.str;
}

size_t disp_get_str_len(disp_val v) {
    if (T(v) != FLAG_STRING) {
	ERRO("disp_get_string failed: %s\n", strerror (errno));
    }
    return D(v)->string_val.len;
}

disp_val disp_make_string(const char *s) {
    disp_val v = ALLOC_TI(FLAG_STRING, 0);
    D(v)->string_val.str = gc_strdup(s);
    D(v)->string_val.len = strlen(s);
    return v;
}
