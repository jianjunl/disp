
#ifndef _STR_H_H
#define _STR_H_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

disp_val disp_make_string(const char *s);
char* disp_get_str(disp_val v);
size_t disp_get_str_len(disp_val v);
#define MKS disp_make_string

#endif // _STR_H_H
