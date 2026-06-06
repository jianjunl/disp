
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
void disp_set_symbol_value_unlock(const disp_env_t *env, disp_val sym, disp_val value);
disp_val disp_make_symbol(disp_sid id);
disp_val disp_make_symbol_by_name(const char *name);
disp_sid disp_get_symbol_id(disp_val v);
char* disp_get_symbol_name(disp_val v);
disp_val disp_get_symbol_value(disp_val v);

#define SYM_ID(v) disp_get_symbol_id(v)
#define SYM_NAME(v) disp_get_name(SYM_ID(v).id)
#define SYM_VALUE(v) disp_get_symbol_value(v)
 
#endif // SYMBOL_H
