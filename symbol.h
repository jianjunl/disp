
#ifndef SYMBOL_H
#define SYMBOL_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- Allocation --- */
disp_box disp_alloc(int flag, disp_data *data);
#define DISP_ALLOC_TI(flag) disp_alloc(flag, gc_typed_calloc(1, sizeof(union disp_data), &union_disp_data_ti))
#define DISP_ALLOC(flag) disp_alloc(flag, gc_calloc(1, sizeof(union disp_data)))

/* --- Symbol table --- */
void disp_init_symbol(void);
void disp_set_symbol_value(disp_box sym, disp_box value);
disp_box disp_make_symbol(const char *name);
char* disp_get_symbol_name(disp_box v);
disp_box disp_get_symbol_value(disp_box v);

#define S disp_get_symbol_name
#define V disp_get_symbol_value
#define T(v) v->flag
#define N(v) v->next
 
#endif // SYMBOL_H
