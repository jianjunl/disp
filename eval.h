
#ifndef EVAL_H
#define EVAL_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- Evaluation --- */
disp_val disp_eval(disp_env_t *env, disp_val expr);
disp_val disp_eval_body(disp_env_t *env, disp_val body);
disp_env_t* disp_get_closure_env(disp_val closure);
 
#endif // EVAL_H
