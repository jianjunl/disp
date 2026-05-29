
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

#if DISP_NAN_BOXING
struct disp_data {
#else // DISP_NAN_BOXING
union disp_data {
#endif // DISP_NAN_BOXING
    struct {
        disp_val car;
        disp_val cdr;
    } cons;
};

#if DISP_NAN_BOXING
GC_STRUCT_TI(disp_data,
#else // DISP_NAN_BOXING
GC_UNION_TI(disp_data,
#endif // DISP_NAN_BOXING
    GC_OFF(disp_data, cons.car),
    GC_OFF(disp_data, cons.cdr)
);

disp_val disp_make_cons(disp_val car, disp_val cdr) {
    disp_val v = ALLOC_TI(FLAG_CONS, 0);
    D(v)->cons.car = car;
    D(v)->cons.cdr = cdr;
    return v;
}

disp_val disp_car(disp_val cons) {
    return (NN(cons) && T(cons) == FLAG_CONS) ? D(cons)->cons.car : NIL;
}

disp_val disp_cdr(disp_val cons) {
    return (NN(cons) && T(cons) == FLAG_CONS) ? D(cons)->cons.cdr : NIL;
}

void disp_set_car(disp_val cons, disp_val car) {
    if (NN(cons) && T(cons) == FLAG_CONS) D(cons)->cons.car = car;
}

void disp_set_cdr(disp_val cons, disp_val cdr) {
    if (NN(cons) && T(cons) == FLAG_CONS) D(cons)->cons.cdr = cdr;
}
