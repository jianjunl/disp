
#ifndef APPLY_H
#define APPLY_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- Apply closures and macros --- */
disp_val disp_apply_closure(disp_val closure, disp_val *args, int arg_count);
disp_val disp_letf(disp_env_t *env, disp_val expr);
void bind_arguments_to_env(disp_env_t *env, disp_val params, disp_val *args, int arg_count);

#endif // APPLY_H
