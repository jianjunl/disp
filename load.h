
#ifndef LOAD_H
#define LOAD_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>

/* --- Loading --- */
disp_box disp_import(const char *filename);
disp_box disp_load(disp_scope_t *env, const char *filename);

#endif // LOAD_H
