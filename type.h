
#ifndef _TYPE_H
#define _TYPE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

disp_val disp_define_type(char *name, disp_val decl);
char* disp_get_type_name(disp_val t);
disp_val disp_get_type_decl(disp_val t);

#endif // _TYPE_H
