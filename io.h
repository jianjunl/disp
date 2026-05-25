
#ifndef IO_H
#define IO_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* --- I/O --- */
disp_box disp_read(FILE *f);
char* disp_string(disp_box v);
char* disp_str(disp_box v);
void disp_write(disp_box v);
void disp_print(disp_box v);
void disp_fprint(FILE *f, disp_box v);
void disp_fwrite(FILE *f, disp_box v);

#endif // IO_H
