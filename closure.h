
#ifndef CLOSURE_H
#define CLOSURE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- Closures and macros --- */
disp_val disp_make_closure(disp_env_t *env, disp_val params, disp_val body, int reuse_env);
disp_val disp_make_macro(disp_env_t *env, disp_val params, disp_val body, int reuse_env);
disp_val disp_get_closure_params(disp_val closure);
disp_val disp_get_closure_body(disp_val closure);
disp_env_t* disp_get_closure_env(disp_val closure);
int disp_is_closure_reuse_env(disp_val closure);

#endif // CLOSURE_H
