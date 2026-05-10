
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
        disp_val *car;
        disp_val *cdr;
    } cons;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, cons.car),
    GC_OFF(disp_data, cons.cdr)
);

disp_val* disp_make_cons(disp_val *car, disp_val *cdr) {
    disp_val *v = DISP_ALLOC_TI(DISP_CONS);
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
