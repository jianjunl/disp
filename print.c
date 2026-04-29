
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

void disp_print(disp_val *v) {
    disp_fprint(stdout, v);
}

void disp_write(disp_val *v) {
    disp_fwrite(stdout, v);
}

char* disp_string(disp_val *v) {
    char *buf = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buf, &size);
    if (!mem) return NULL;
    disp_fprint(mem, v);
    fclose(mem);
    return buf;   // 调用者负责 free
}

char* disp_str(disp_val *v) {
    char *buf = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buf, &size);
    if (!mem) return NULL;
    disp_fwrite(mem, v);
    fclose(mem);
    return buf;   // 调用者负责 free
}

void disp_fprint(FILE *out, disp_val *v) {
    if (!v) {
        fprintf(out, "nil");
        return;
    }
    switch (T(v)) {
        case DISP_TYPE:   fprintf(out, "%s",         disp_get_type_name(v));     break;
        case DISP_BYTE:   fprintf(out, "%d",         disp_get_byte(v));          break;
        case DISP_SHORT:  fprintf(out, "%d",         disp_get_short(v));         break;
        case DISP_INT:    fprintf(out, "%d",         disp_get_int(v));           break;
        case DISP_LONG:   fprintf(out, "%ld",        disp_get_long(v));          break;
        case DISP_FLOAT:  fprintf(out, "%g",         (double)disp_get_float(v)); break;
        case DISP_DOUBLE: fprintf(out, "%g",         disp_get_double(v));        break;
        case DISP_STRING: fprintf(out, "\"%s\"",     disp_get_str(v));      break;
        case DISP_SYMBOL: fprintf(out, "%s",         disp_get_symbol_name(v));   break;
        case DISP_CORO:   fprintf(out, "#<coroutine>"); break;
        case DISP_CHAN:   fprintf(out, "#<channel>"); break;
        case DISP_FILE: {
            FILE *f = disp_get_file(v);
            if (f == NULL) fprintf(out, "#<file (closed)>");
            else if (f == stdin) fprintf(out, "#<file stdin>");
            else if (f == stdout) fprintf(out, "#<file stdout>");
            else if (f == stderr) fprintf(out, "#<file stderr>");
            else fprintf(out, "#<file %p>", f);
            break;
        }
        case DISP_CONS:
            fputc('(', out);
            disp_fprint(out, disp_car(v));
            disp_val *rest = disp_cdr(v);
            while (rest && T(rest) == DISP_CONS) {
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
        case DISP_BUILTIN:  fprintf(out, "#<builtin>"); break;
        case DISP_SYSCALL:  fprintf(out, "#<function>"); break;
        case DISP_VOID: {
            if (v == NIL) fprintf(out, "nil");
            else fprintf(out, "true");
        }; break;
        case DISP_CLOSURE: fprintf(out, "#<closure>"); break;
        case DISP_MACRO:   fprintf(out, "#<macro>");   break;
        case DISP_THREAD:  fprintf(out, "#<thread>");  break;
        case DISP_MUTEX:   fprintf(out, "#<mutex>");   break;
        case DISP_COND:    fprintf(out, "#<cond>");    break;
        case DISP_SOCKET:
            fprintf(out, "#<socket %d>", disp_get_socket_fd(v));
            break;
        default:         fprintf(out, "#<unknown>");
    }
}

void disp_fwrite(FILE *out, disp_val *v) {
    if (!v) {
        fprintf(out, "nil");
        return;
    }
    switch (T(v)) {
        case DISP_TYPE:   fprintf(out, "%s",         disp_get_type_name(v));     break;
        case DISP_BYTE:   fprintf(out, "%d",         disp_get_byte(v));          break;
        case DISP_SHORT:  fprintf(out, "%d",         disp_get_short(v));         break;
        case DISP_INT:    fprintf(out, "%d",         disp_get_int(v));           break;
        case DISP_LONG:   fprintf(out, "%ld",        disp_get_long(v));          break;
        case DISP_FLOAT:  fprintf(out, "%g",         (double)disp_get_float(v)); break;
        case DISP_DOUBLE: fprintf(out, "%g",         disp_get_double(v));        break;
        case DISP_STRING: fprintf(out, "%s",         disp_get_str(v));           break;
        case DISP_SYMBOL: fprintf(out, "%s",         disp_get_symbol_name(v));   break;
        case DISP_CORO:   fprintf(out, "#<coroutine>"); break;
        case DISP_CHAN:   fprintf(out, "#<channel>"); break;
        case DISP_FILE: {
            FILE *f = disp_get_file(v);
            if (f == NULL) fprintf(out, "#<file (closed)>");
            else if (f == stdin) fprintf(out, "#<file stdin>");
            else if (f == stdout) fprintf(out, "#<file stdout>");
            else if (f == stderr) fprintf(out, "#<file stderr>");
            else fprintf(out, "#<file %p>", f);
            break;
        }
        case DISP_CONS:
            fputc('(', out);
            disp_fwrite(out, disp_car(v));
            disp_val *rest = disp_cdr(v);
            while (rest && T(rest) == DISP_CONS) {
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
        case DISP_SYSCALL:  fprintf(out, "#<function>"); break;
        case DISP_BUILTIN:  fprintf(out, "#<builtin>"); break;
        case DISP_VOID: {
            if (v == NIL) fprintf(out, "nil");
            else fprintf(out, "true");
        }; break;
        case DISP_CLOSURE: fprintf(out, "#<closure>"); break;
        case DISP_MACRO:   fprintf(out, "#<macro>");   break;
        case DISP_THREAD:  fprintf(out, "#<thread>");  break;
        case DISP_MUTEX:   fprintf(out, "#<mutex>");   break;
        case DISP_COND:    fprintf(out, "#<cond>");    break;
        case DISP_SOCKET:
            fprintf(out, "#<socket %d>", disp_get_socket_fd(v));
            break;
        default:         fprintf(out, "#<unknown>");
    }
}
