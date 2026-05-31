
#ifndef CLOSURE_H
#define CLOSURE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- Closures and macros --- */
void disp_closure_reuse(disp_val closure);
disp_val disp_make_closure(disp_env_t *env, disp_val params, disp_val body, int reuse_env);
disp_val disp_make_macro(disp_env_t *env, disp_val params, disp_val body, int reuse_env);
disp_val disp_get_closure_params(disp_val closure);
disp_val disp_get_closure_body(disp_val closure);
disp_val disp_apply_closure(disp_val closure, disp_val *args, int arg_count);
disp_val disp_letf(disp_env_t *env, disp_val expr);
void bind_arguments_to_env(disp_env_t *env, disp_val params, disp_val *args, int arg_count);

#endif // CLOSURE_H
