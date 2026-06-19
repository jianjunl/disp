
#ifndef _SCOPE_H
#define _SCOPE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

disp_val disp_new_scope(disp_val parent);

disp_env_t* disp_get_scope_env(disp_val v);

#endif // _SCOPE_H
