
#ifndef FUNC_H
#define FUNC_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* Function pointer type for built‑in functions */
typedef disp_box (*disp_syscall_t)(disp_box * args, int arg_count);
typedef disp_box (*disp_builtin_t)(disp_scope_t *scope, disp_box arg);

disp_builtin_t disp_get_builtin(disp_box v);
char* disp_get_builtin_desc(disp_box v);
disp_syscall_t disp_get_syscall(disp_box v);
char* disp_get_syscall_desc(disp_box v);
disp_box disp_make_syscall(disp_syscall_t f, char *d);
disp_box disp_make_builtin(disp_builtin_t f, char *d);

#define MKB disp_make_builtin
#define MKF disp_make_syscall
#define MKS disp_make_string

#endif // FUNC_H
