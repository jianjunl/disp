
#ifndef FILE_H
#define FILE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

extern disp_val disp_make_file(FILE *f, char *mode);
extern FILE* disp_get_file(disp_val v);
extern char* disp_get_file_mode(disp_val v);
extern void disp_set_file(disp_val v, FILE *f);
extern void disp_set_file_mode(disp_val v, char *mode);

#endif // FILE_H
