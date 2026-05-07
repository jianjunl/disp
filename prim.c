
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
    /* 基本数值类型 */
    char byte_val;
    short short_val;
    int int_val;
    long long_val;
    float float_val;
    double double_val;

    /* 字符串 */
    struct {
        char *str;
        size_t len;
    } string_val;

    /* cons 单元 */
    struct {
        disp_val *car;
        disp_val *cdr;
    } cons;

    /* 套接字 */
    struct {
        int fd;
    } socket_val;
};

char disp_get_byte(disp_val *v) {
    if (v->flag != DISP_BYTE) {
	ERRO("disp_get_byte failed: %s\n", strerror (errno));
    }
    return v->data->byte_val;
}

short disp_get_short(disp_val *v) {
    if (v->flag != DISP_SHORT) {
	ERRO("disp_get_short failed: %s\n", strerror (errno));
    }
    return v->data->short_val;
}

int disp_get_int(disp_val *v) {
    if (v->flag != DISP_INT) {
	ERRO("disp_get_int failed: %s\n", strerror (errno));
    }
    return v->data->int_val;
}

long disp_get_long(disp_val *v) {
    if (v->flag != DISP_LONG) {
	ERRO("disp_get_long failed: %s\n", strerror (errno));
    }
    return v->data->long_val;
}

float disp_get_float(disp_val *v) {
    if (v->flag != DISP_FLOAT) {
	ERRO("disp_get_float failed: %s\n", strerror (errno));
    }
    return v->data->float_val;
}

double disp_get_double(disp_val *v) {
    if (v->flag != DISP_DOUBLE) {
	ERRO("disp_get_double failed: %s\n", strerror (errno));
    }
    return v->data->double_val;
}

char* disp_get_str(disp_val *v) {
    if (v->flag != DISP_STRING) {
	ERRO("disp_get_string failed: %s\n", strerror (errno));
    }
    return v->data->string_val.str;
}

size_t disp_get_str_len(disp_val *v) {
    if (v->flag != DISP_STRING) {
	ERRO("disp_get_string failed: %s\n", strerror (errno));
    }
    return v->data->string_val.len;
}

disp_val* disp_make_byte(char c) {
    disp_val *v = DISP_ALLOC(DISP_BYTE);
    v->data->byte_val = c;
    return v;
}

disp_val* disp_make_short(short i) {
    disp_val *v = DISP_ALLOC(DISP_SHORT);
    v->data->int_val = i;
    return v;
}

disp_val* disp_make_int(int i) {
    disp_val *v = DISP_ALLOC(DISP_INT);
    v->data->int_val = i;
    return v;
}

disp_val* disp_make_long(long l) {
    disp_val *v = DISP_ALLOC(DISP_LONG);
    v->data->long_val = l;
    return v;
}

disp_val* disp_make_float(float f) {
    disp_val *v = DISP_ALLOC(DISP_FLOAT);
    v->data->float_val = f;
    return v;
}

disp_val* disp_make_double(double d) {
    disp_val *v = DISP_ALLOC(DISP_DOUBLE);
    v->data->double_val = d;
    return v;
}

disp_val* disp_make_string(const char *s) {
    disp_val *v = DISP_ALLOC(DISP_STRING);
    v->data->string_val.str = gc_strdup(s);
    v->data->string_val.len = strlen(s);
    return v;
}

disp_val* disp_make_cons(disp_val *car, disp_val *cdr) {
    disp_val *v = DISP_ALLOC(DISP_CONS);
    v->data->cons.car = car;
    v->data->cons.cdr = cdr;
    return v;
}

disp_val* disp_car(disp_val *cons) {
    return (cons && cons->flag == DISP_CONS) ? cons->data->cons.car : NIL;
}

disp_val* disp_cdr(disp_val *cons) {
    return (cons && cons->flag == DISP_CONS) ? cons->data->cons.cdr : NIL;
}

void disp_set_car(disp_val *cons, disp_val *car) {
    if (cons && cons->flag == DISP_CONS) GC_ASSIGN_PTR(cons->data->cons.car, car);
}

void disp_set_cdr(disp_val *cons, disp_val *cdr) {
    if (cons && cons->flag == DISP_CONS) GC_ASSIGN_PTR(cons->data->cons.cdr, cdr);
}

disp_val* disp_make_socket(int fd) {
    disp_val *v = DISP_ALLOC(DISP_SOCKET);
    v->data->socket_val.fd = fd;
    return v;
}

int disp_get_socket_fd(disp_val *v) {
    if (v->flag != DISP_SOCKET) return -1;
    return v->data->socket_val.fd;
}
