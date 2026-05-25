
#ifndef EVAL_H
#define EVAL_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- Evaluation --- */
disp_box disp_eval(disp_scope_t *scope, disp_box expr);
disp_box disp_eval_body(disp_scope_t *scope, disp_box body);
disp_scope_t* disp_get_closure_env(disp_box closure);
 
#endif // EVAL_H
