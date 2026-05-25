
#ifndef PRIM_H
#define PRIM_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

disp_box disp_make_int(int i);
disp_box disp_make_long(long l);
char disp_get_byte(disp_box v);
short disp_get_short(disp_box v);
int disp_get_int(disp_box v);
long disp_get_long(disp_box v);
disp_box disp_make_float(float f);
disp_box disp_make_double(double d);
float disp_get_float(disp_box v);
double disp_get_double(disp_box v);
disp_box disp_make_string(const char *s);
char* disp_get_str(disp_box v);
size_t disp_get_str_len(disp_box v);
disp_box disp_define_type(char *name, disp_box decl);
char* disp_get_type_name(disp_box t);
disp_box disp_get_type_decl(disp_box t);

#endif // PRIM_H
