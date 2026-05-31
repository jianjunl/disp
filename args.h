
#ifndef ARGS_H
#define ARGS_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

disp_val disp_make_args(int argc, char** argv);
int disp_get_argc(disp_val v);
char** disp_get_argv(disp_val v);

#endif // ARGS_H
