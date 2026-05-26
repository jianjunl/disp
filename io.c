
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
//#define DEBUG 1
#include "disp.h"

/* ======================== Printer ======================== */

void disp_print(disp_box v) {
    disp_fprint(stdout, v);
}

void disp_write(disp_box v) {
    disp_fwrite(stdout, v);
}

char* disp_string(disp_box v) {
    char *buf = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buf, &size);
    if (!mem) return NULL;
    disp_fprint(mem, v);
    fclose(mem);
    char *s = gc_strdup(buf);
    free(buf);
    return s;
}

char* disp_str(disp_box v) {
    char *buf = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buf, &size);
    if (!mem) return NULL;
    disp_fwrite(mem, v);
    fclose(mem);
    char *s = gc_strdup(buf);
    free(buf);
    return s;
}

void disp_fprint(FILE *out, disp_box v) {
    if (!v) {
        fprintf(out, "nil");
        return;
    }
    switch (T(v)) {
        case FLAG_BYTE:   fprintf(out, "%d",         disp_get_byte(v));          break;
        case FLAG_SHORT:  fprintf(out, "%d",         disp_get_short(v));         break;
        case FLAG_INT:    fprintf(out, "%d",         disp_get_int(v));           break;
        case FLAG_LONG:   fprintf(out, "%ld",        disp_get_long(v));          break;
        case FLAG_FLOAT:  fprintf(out, "%g",         (double)disp_get_float(v)); break;
        case FLAG_DOUBLE: fprintf(out, "%g",         disp_get_double(v));        break;
        case FLAG_STRING: fprintf(out, "\"%s\"",     disp_get_str(v));           break;
        case FLAG_SYMBOL: fprintf(out, "%s",         disp_get_symbol_name(v));   break;
        case TAG_TYPE:    fprintf(out, "%s",         disp_get_type_name(v));     break;
        case TAG_CORO:    fprintf(out, "#<coroutine>"); break;
        case TAG_CHAN:    fprintf(out, "#<channel>"); break;
        case TAG_FILE: {
            FILE *f = disp_get_file(v);
            if (f == NULL) fprintf(out, "#<file (closed)>");
            else if (f == stdin) fprintf(out, "#<file stdin>");
            else if (f == stdout) fprintf(out, "#<file stdout>");
            else if (f == stderr) fprintf(out, "#<file stderr>");
            else fprintf(out, "#<file %p>", f);
            break;
        }
        case FLAG_CONS:
            fputc('(', out);
            disp_fprint(out, disp_car(v));
            disp_box rest = disp_cdr(v);
            while (rest && T(rest) == FLAG_CONS) {
                fputc(' ', out);
                disp_fprint(out, disp_car(rest));
                rest = disp_cdr(rest);
            }
            if (rest && rest != NIL) {
                fprintf(out, " . ");
                disp_fprint(out, rest);
            }
            fputc(')', out);
            break;
        case FLAG_VOID: v == TRUE ? fprintf(out, "true") : fprintf(out, "nil"); break;
        case FLAG_NAN: fprintf(out, "NaN"); break;
        case FLAG_BUILTIN:  fprintf(out, "#<builtin>"); break;
        case FLAG_SYSCALL:  fprintf(out, "#<function>"); break;
        case FLAG_CLOSURE:  fprintf(out, "#<closure>"); break;
        case FLAG_MACRO:    fprintf(out, "#<macro>"); break;
        case TAG_THREAD:   fprintf(out, "#<thread>"); break;
        case TAG_MUTEX:    fprintf(out, "#<mutex>"); break;
        case TAG_COND:     fprintf(out, "#<cond>"); break;
        case TAG_SOCKET:
            fprintf(out, "#<socket %d>", disp_get_socket_fd(v));
            break;
        default:         fprintf(out, "#<unknown>");
    }
}

void disp_fwrite(FILE *out, disp_box v) {
    if (!v) {
        fprintf(out, "nil");
        return;
    }
    switch (T(v)) {
        case FLAG_BYTE:   fprintf(out, "%d",         disp_get_byte(v));          break;
        case FLAG_SHORT:  fprintf(out, "%d",         disp_get_short(v));         break;
        case FLAG_INT:    fprintf(out, "%d",         disp_get_int(v));           break;
        case FLAG_LONG:   fprintf(out, "%ld",        disp_get_long(v));          break;
        case FLAG_FLOAT:  fprintf(out, "%g",         (double)disp_get_float(v)); break;
        case FLAG_DOUBLE: fprintf(out, "%g",         disp_get_double(v));        break;
        case FLAG_STRING: fprintf(out, "%s",         disp_get_str(v));           break;
        case FLAG_SYMBOL: fprintf(out, "%s",         disp_get_symbol_name(v));   break;
        case TAG_TYPE:    fprintf(out, "%s",         disp_get_type_name(v));     break;
        case TAG_CORO:    fprintf(out, "#<coroutine>"); break;
        case TAG_CHAN:    fprintf(out, "#<channel>"); break;
        case TAG_FILE: {
            FILE *f = disp_get_file(v);
            if (f == NULL) fprintf(out, "#<file (closed)>");
            else if (f == stdin) fprintf(out, "#<file stdin>");
            else if (f == stdout) fprintf(out, "#<file stdout>");
            else if (f == stderr) fprintf(out, "#<file stderr>");
            else fprintf(out, "#<file %p>", f);
            break;
        }
        case FLAG_CONS:
            fputc('(', out);
            disp_fwrite(out, disp_car(v));
            disp_box rest = disp_cdr(v);
            while (rest && T(rest) == FLAG_CONS) {
                fputc(' ', out);
                disp_fwrite(out, disp_car(rest));
                rest = disp_cdr(rest);
            }
            if (rest && rest != NIL) {
                fprintf(out, " . ");
                disp_fwrite(out, rest);
            }
            fputc(')', out);
            break;
        case FLAG_VOID: v == TRUE ? fprintf(out, "true") : fprintf(out, "nil"); break;
        case FLAG_NAN: fprintf(out, "NaN"); break;
        case FLAG_SYSCALL:  fprintf(out, "#<function>"); break;
        case FLAG_BUILTIN:  fprintf(out, "#<builtin>"); break;
        case FLAG_CLOSURE: fprintf(out, "#<closure>"); break;
        case FLAG_MACRO:   fprintf(out, "#<macro>");   break;
        case TAG_THREAD:  fprintf(out, "#<thread>");  break;
        case TAG_MUTEX:   fprintf(out, "#<mutex>");   break;
        case TAG_COND:    fprintf(out, "#<cond>");    break;
        case TAG_SOCKET:
            fprintf(out, "#<socket %d>", disp_get_socket_fd(v));
            break;
        default:         fprintf(out, "#<unknown>");
    }
}
