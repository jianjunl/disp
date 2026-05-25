
#ifndef CLOSURE_H
#define CLOSURE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- Closures and macros --- */
void disp_closure_reuse(disp_box closure);
disp_box disp_make_closure(disp_scope_t *env, disp_box params, disp_box body, int reuse_scope);
disp_box disp_make_macro(disp_scope_t *env, disp_box params, disp_box body, int reuse_scope);
disp_box disp_get_closure_params(disp_box closure);
disp_box disp_get_closure_body(disp_box closure);
disp_box disp_apply_closure(disp_box closure, disp_box *args, int arg_count);
disp_box disp_letf(disp_scope_t *scope, disp_box expr);
void bind_arguments_to_scope(disp_scope_t *scope, disp_box params, disp_box *args, int arg_count);

#endif // CLOSURE_H
