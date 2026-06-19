
#ifndef _NUMBER_H
#define _NUMBER_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

char disp_get_byte(disp_val v);
short disp_get_short(disp_val v);
int disp_get_int(disp_val v);
long disp_get_long(disp_val v);
float disp_get_float(disp_val v);
double disp_get_double(disp_val v);
disp_val disp_make_byte(char c);
disp_val disp_make_short(short s);
disp_val disp_make_int(int i);
disp_val disp_make_long(long l);
disp_val disp_make_float(float f);
disp_val disp_make_double(double d);

#endif // _NUMBER_H
