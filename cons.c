
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
        disp_box car;
        disp_box cdr;
    } cons;
};

GC_UNION_TI(disp_data,
    GC_OFF(disp_data, cons.car),
    GC_OFF(disp_data, cons.cdr)
);

disp_box disp_make_cons(disp_box car, disp_box cdr) {
    disp_box v = ALLOC_TI(FLAG_CONS);
    GC_ASSIGN_PTR(v->data->cons.car, car);
    GC_ASSIGN_PTR(v->data->cons.cdr, cdr);
    return v;
}

disp_box disp_car(disp_box cons) {
    return (cons && cons->flag == FLAG_CONS) ? cons->data->cons.car : NIL;
}

disp_box disp_cdr(disp_box cons) {
    return (cons && cons->flag == FLAG_CONS) ? cons->data->cons.cdr : NIL;
}

void disp_set_car(disp_box cons, disp_box car) {
    if (cons && cons->flag == FLAG_CONS) GC_ASSIGN_PTR(cons->data->cons.car, car);
}

void disp_set_cdr(disp_box cons, disp_box cdr) {
    if (cons && cons->flag == FLAG_CONS) GC_ASSIGN_PTR(cons->data->cons.cdr, cdr);
}
