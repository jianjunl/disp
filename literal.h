
#ifndef _LITERAL_H
#define _LITERAL_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- numbers --- */
bool disp_is_float_literal(const char *s);
bool disp_is_double_literal(const char *s);
bool disp_is_long_literal(const char *s);
bool disp_is_integer_literal(const char *s);
disp_val disp_parse_number(const char *s);

#endif // _LITERAL_H
