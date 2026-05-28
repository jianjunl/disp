
#ifndef CONS_H
#define CONS_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- Accessors (for cons cells) --- */
disp_val disp_car(disp_val cons);
disp_val disp_cdr(disp_val cons);
void disp_set_car(disp_val cons, disp_val car);   /* use with care – immutable by convention */
void disp_set_cdr(disp_val cons, disp_val cdr);
disp_val disp_make_cons(disp_val car, disp_val cdr);

#endif // CONS_H
