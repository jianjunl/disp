
#ifndef _TYPE_H
#define _TYPE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

disp_val disp_convert_to_byte(disp_val v);
disp_val disp_convert_to_short(disp_val v);
disp_val disp_convert_to_int(disp_val v);
disp_val disp_convert_to_long(disp_val v);
disp_val disp_convert_to_float(disp_val v);
disp_val disp_convert_to_double(disp_val v);

disp_val disp_new_type(disp_val parent);

disp_env_t* disp_get_type_env(disp_val v);
#endif // _TYPE_H
