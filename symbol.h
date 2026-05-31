
#ifndef SYMBOL_H
#define SYMBOL_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- Allocation --- */

#define ALLOC_TI(f, t) V(f, t, gc_typed_calloc(1, sizeof(disp_data), &struct_disp_data_ti))
#define ALLOC(f, t) V(f, t, gc_calloc(1, sizeof(disp_data)))

/* --- Symbol table --- */
void disp_init_symbol(void);
void disp_set_symbol_value(disp_val sym, disp_val value);
disp_val disp_make_symbol(const char *name);
uint64_t disp_get_symbol_id(disp_val v);
char* disp_get_symbol_name(disp_val v);
disp_val disp_get_symbol_value(disp_val v);

#define SN(v) disp_get_symbol_name(v)
#define SV(v) disp_get_symbol_value(v)
 
#endif // SYMBOL_H
