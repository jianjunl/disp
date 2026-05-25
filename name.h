
#ifndef NAME_H
#define NAME_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

void disp_init_name_table(void);
uint64_t disp_get_id(const char *name);
const char* disp_get_name(uint64_t id);

#endif // NAME_H
