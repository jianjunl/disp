
#ifndef CONS_H
#define CONS_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- Accessors (for cons cells) --- */
disp_box disp_car(disp_box cons);
disp_box disp_cdr(disp_box cons);
void disp_set_car(disp_box cons, disp_box car);   /* use with care – immutable by convention */
void disp_set_cdr(disp_box cons, disp_box cdr);
disp_box disp_make_cons(disp_box car, disp_box cdr);

#endif // CONS_H
