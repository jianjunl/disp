
#ifndef FILE_H
#define FILE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

extern disp_box disp_make_file(FILE *f, char *mode);
extern FILE* disp_get_file(disp_box v);
extern char* disp_get_file_mode(disp_box v);
extern void disp_set_file(disp_box v, FILE *f);
extern void disp_set_file_mode(disp_box v, char *mode);

#endif // FILE_H
