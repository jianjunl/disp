
#ifndef _NUMBER_H
#define _NUMBER_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

disp_val disp_make_int(int i);
disp_val disp_make_long(long l);
char disp_get_byte(disp_val v);
short disp_get_short(disp_val v);
int disp_get_int(disp_val v);
long disp_get_long(disp_val v);
disp_val disp_make_float(float f);
disp_val disp_make_double(double d);
float disp_get_float(disp_val v);
double disp_get_double(disp_val v);
disp_val disp_make_string(const char *s);
char* disp_get_str(disp_val v);
size_t disp_get_str_len(disp_val v);
disp_val disp_define_type(char *name, disp_val decl);
char* disp_get_type_name(disp_val t);
disp_val disp_get_type_decl(disp_val t);

#endif // _NUMBER_H
