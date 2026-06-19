
#ifndef IO_H
#define IO_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- I/O --- */
disp_val disp_read_lisp(FILE *f);
char* disp_string(disp_val v);
char* disp_str(disp_val v);
void disp_write(disp_val v);
void disp_print(disp_val v);
void disp_fprint(FILE *f, disp_val v);
void disp_fwrite(FILE *f, disp_val v);

#endif // IO_H
