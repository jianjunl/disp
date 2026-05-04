
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

// 辅助函数：将 disp_val 转换为整数（用于 %d, %x, %o 等）
static intmax_t val_to_int(disp_val *v) {
    if (v == NIL) return 0;
    if (v == TRUE) return 1;
    switch (T(v)) {
        case DISP_BYTE:   return disp_get_byte(v);
        case DISP_SHORT:  return disp_get_short(v);
        case DISP_INT:    return disp_get_int(v);
        case DISP_LONG:   return disp_get_long(v);
        case DISP_FLOAT:  return (intmax_t)disp_get_float(v);
        case DISP_DOUBLE: return (intmax_t)disp_get_double(v);
        default:          return 0;
    }
}

// 辅助函数：将 disp_val 转换为字符串（用于 %s）
static const char* val_to_str(disp_val *v) {
    if (v == NIL) return "nil";
    if (v == TRUE) return "true";
    if (T(v) == DISP_STRING) return disp_get_str(v);
    static char buf[256];
    char *s = disp_str(v);
    strncpy(buf, s, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    gc_free(s);
    return buf;
}

// 辅助函数：将 disp_val 转换为双精度浮点数（用于 %f, %g, %e）
static double val_to_double(disp_val *v) {
    switch (T(v)) {
        case DISP_BYTE:   return disp_get_byte(v);
        case DISP_SHORT:  return disp_get_short(v);
        case DISP_INT:    return disp_get_int(v);
        case DISP_LONG:   return disp_get_long(v);
        case DISP_FLOAT:  return disp_get_float(v);
        case DISP_DOUBLE: return disp_get_double(v);
        default:          if(v == NIL) return 0.0; else ERET(0.0, "NaN:%s", val_to_str(v));
    }
}

// 修正后的 disp_vprintf
static int disp_vprintf(FILE *out, const char *fmt, disp_val **args, int arg_count) {
    int printed = 0;
    int arg_idx = 0;
    const char *p = fmt;
    while (*p) {
        if (*p != '%') {
            fputc(*p, out);
            printed++;
            p++;
            continue;
        }
        p++;
        if (*p == '%') {
            fputc('%', out);
            printed++;
            p++;
            continue;
        }

        // 收集标志
        char flags[8] = {0};
        int fi = 0;
        while (strchr("-+ 0#", *p)) {
            if (fi < 6) flags[fi++] = *p;
            p++;
        }
        // 宽度
        char width_str[16] = {0};
        if (isdigit(*p)) {
            int wi = 0;
            while (isdigit(*p) && wi < 15) width_str[wi++] = *p++;
            width_str[wi] = '\0';
        }
        // 精度
        char prec_str[16] = {0};
        if (*p == '.') {
            p++;
            if (isdigit(*p)) {
                int pi = 0;
                prec_str[pi++] = '.';           // 先加 '.'
                while (isdigit(*p) && pi < 14) prec_str[pi++] = *p++;
                prec_str[pi] = '\0';
            } else {
                strcpy(prec_str, ".");
            }
        }
        // 跳过长度修饰符
        while (strchr("hljztL", *p)) p++;
        char spec = *p;
        if (spec == '\0') break;
        p++;

        if (arg_idx >= arg_count) {
            fprintf(stderr, "disp_vprintf: missing argument for format specifier\n");
            return -1;
        }
        disp_val *arg = args[arg_idx++];

        // 构建完整格式字符串，例如 "%+10.2f"
        char fmt_buf[64];
        int len = snprintf(fmt_buf, sizeof(fmt_buf), "%%%s%s%s%c",
                          flags, width_str, prec_str, spec);
        if (len < 0 || len >= (int)sizeof(fmt_buf)) {
            // 回退
            snprintf(fmt_buf, sizeof(fmt_buf), "%%%c", spec);
        }

        switch (spec) {
            case 'd': case 'i': {
                intmax_t val = val_to_int(arg);
                printed += fprintf(out, fmt_buf, val);
                break;
            }
            case 'u': case 'x': case 'X': case 'o': {
                uintmax_t val = (uintmax_t)val_to_int(arg);
                printed += fprintf(out, fmt_buf, val);
                break;
            }
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
                double val = val_to_double(arg);
                printed += fprintf(out, fmt_buf, val);
                break;
            }
            case 'c': {
                int ch = (int)val_to_int(arg);
                printed += fprintf(out, fmt_buf, ch);
                break;
            }
            case 's': {
                const char *str = val_to_str(arg);
                printed += fprintf(out, fmt_buf, str);
                break;
            }
            case 'p': {
                printed += fprintf(out, fmt_buf, (void*)arg);
                break;
            }
            default:
                fputc('%', out);
                fputc(spec, out);
                printed += 2;
                break;
        }
    }
    if (arg_idx < arg_count) {
        fprintf(stderr, "disp_vprintf: %d unused argument(s)\n", arg_count - arg_idx);
    }
    return printed;
}

static disp_val* printf_syscall(disp_val **args, int count) {
    if (count < 1 || T(args[0]) != DISP_STRING) {
        ERET(NIL, "printf: format string required");
    }
    const char *fmt = disp_get_str(args[0]);
    int printed = disp_vprintf(stdout, fmt, args + 1, count - 1);
    if (printed < 0) return NIL;
    return TRUE;   // 返回 true 而不是打印数字
}

static disp_val* fprintf_syscall(disp_val **args, int count) {
    if (count < 2 || T(args[0]) != DISP_FILE || T(args[1]) != DISP_STRING) {
        ERET(NIL, "fprintf: (file format-string ...) required");
    }
    FILE *out = disp_get_file(args[0]);
    const char *fmt = disp_get_str(args[1]);
    int printed = disp_vprintf(out, fmt, args + 2, count - 2);
    if (printed < 0) return NIL;
    return TRUE;
}

// --- write --- (prints the evaluated argument, returns it)
static disp_val* write_syscall(disp_val **args, int count) {
    if (count < 1) {
        ERET(NIL, "writeln: expected at least one argument");
    }
    for (int i = 0; i < count; i++) {
        disp_write(args[i]);
        if (i < count - 1) printf(" ");   // 参数之间加空格分隔
    }
    return (count > 0) ? args[count-1] : NIL;
}

// --- writeln --- (print followed by newline)
static disp_val* writeln_syscall(disp_val **args, int count) {
    if (count < 1) {
        ERET(NIL, "writeln: expected at least one argument");
    }
    for (int i = 0; i < count; i++) {
        disp_write(args[i]);
        if (i < count - 1) printf(" ");   // 参数之间加空格分隔
    }
    printf("\n");
    return (count > 0) ? args[count-1] : NIL;
}

// --- print --- (prints the evaluated argument, returns it)
static disp_val* print_syscall(disp_val **args, int count) {
    if (count < 1) {
        ERET(NIL, "print: expected at least one argument");
    }
    for (int i = 0; i < count; i++) {
        disp_print(args[i]);
        if (i < count - 1) printf(" ");   // 参数之间加空格分隔
    }
    return (count > 0) ? args[count-1] : NIL;
}

// --- println --- (print followed by newline)
static disp_val* println_syscall(disp_val **args, int count) {
    if (count < 1) {
        ERET(NIL, "println: expected at least one argument");
    }
    for (int i = 0; i < count; i++) {
        disp_print(args[i]);
        if (i < count - 1) printf(" ");   // 参数之间加空格分隔
    }
    printf("\n");
    return (count > 0) ? args[count-1] : NIL;
}

// --- system --- (execute shell command)
static disp_val* system_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_STRING) {
        ERET(NIL, "system: expected a string argument");
    }
    int ret = system(disp_get_str(args[0]));
    return disp_make_int(ret);
}

static disp_val* sleep_syscall(disp_val **args, int count) {
    if (count != 1) {
        ERET(NIL, "sleep: expected one argument (seconds)");
    }
    double seconds = val_to_double(args[0]);
    struct timespec req, rem;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (seconds - req.tv_sec) * 1000000000.0;
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem;
    }
    return NIL;  // 返回 nil
}

// --- time ---
static disp_val* time_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (rest == NIL) {
        // (time) -> 返回当前时间字符串
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char buf[128];
        strftime(buf, sizeof(buf), "%c", tm);
        return disp_make_string(buf);
    } else {
        // (time expr) -> 执行并计时
        if (disp_cdr(rest) != NIL) {
            ERRO("time: warning - extra arguments ignored");
        }
        disp_val *form = disp_car(rest);
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        disp_val *result = disp_eval(form);
        clock_gettime(CLOCK_MONOTONIC, &end);
        long long diff_ns = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
        double diff_ms = diff_ns / 1000000.0;
        printf(";; Elapsed time: %.3f ms\n", diff_ms);
        return result;
    }
}

/* 全局 I/O 互斥锁，保护所有线程对文件流的写入 */
static gc_mutex_t io_mutex = GC_PTHREAD_MUTEX_INITIALIZER;

/* (safe-fprintf file format-string args...) -> result */
static disp_val* safe_fprintf_syscall(disp_val **args, int count) {
    if (count < 2 || T(args[0]) != DISP_FILE || T(args[1]) != DISP_STRING) {
        ERET(NIL, "safe-fprintf: (file format-string ...) required");
    }
    FILE *out = disp_get_file(args[0]);
    if (!out) return NIL;
    const char *fmt = disp_get_str(args[1]);
    
    gc_pthread_mutex_lock(&io_mutex);
    int printed = disp_vprintf(out, fmt, args + 2, count - 2);
    gc_pthread_mutex_unlock(&io_mutex);
    
    if (printed < 0) return NIL;
    return TRUE;
}

/* ======================== pretty-print ======================== */

static void pretty_print_indent(int indent) {
    for (int i = 0; i < indent; i++) fputc(' ', stdout);
}

static void pretty_print_obj(disp_val *v, int indent, int newline) {
    if (!v) {
        printf("nil");
        return;
    }
    switch (T(v)) {
        case DISP_CONS: {
            printf("(");
            disp_val *p = v;
            int first = 1;
            while (p != NIL && T(p) == DISP_CONS) {
                if (!first) {
                    if (newline) {
                        printf("\n");
                        pretty_print_indent(indent + 2);
                    } else {
                        printf(" ");
                    }
                }
                pretty_print_obj(disp_car(p), indent + 2, newline);
                first = 0;
                p = disp_cdr(p);
            }
            if (p != NIL) {
                printf(" . ");
                pretty_print_obj(p, indent + 2, newline);
            }
            printf(")");
            break;
        }
        case DISP_STRING:
            printf("\"%s\"", disp_get_str(v));
            break;
        case DISP_SYMBOL:
            printf("%s", disp_get_symbol_name(v));
            break;
        case DISP_INT:
            printf("%d", disp_get_int(v));
            break;
        case DISP_LONG:
            printf("%ld", disp_get_long(v));
            break;
        case DISP_FLOAT:
            printf("%g", disp_get_float(v));
            break;
        case DISP_DOUBLE:
            printf("%g", disp_get_double(v));
            break;
        case DISP_VOID:
            printf(v == NIL ? "nil" : "true");
            break;
        default:
            disp_print(v);
            break;
    }
}

static disp_val* pretty_print_syscall(disp_val **args, int count) {
    if (count < 1) ERET(NIL, "pretty-print: expected at least one argument");
    for (int i = 0; i < count; i++) {
        pretty_print_obj(args[i], 0, 1);
        if (i < count - 1) printf(" ");
    }
    printf("\n");
    return (count > 0) ? args[count-1] : NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("write"  , MKF(write_syscall   , "<write>"  ), 1);
    DEF("writeln", MKF(writeln_syscall , "<writeln>"), 1);
    DEF("print"  , MKF(print_syscall   , "<print>"  ), 1);
    DEF("println", MKF(println_syscall , "<println>"), 1);
    DEF("printf" , MKF(printf_syscall  , "<printf>" ), 1);
    DEF("fprintf", MKF(fprintf_syscall , "<fprintf>"), 1);
    DEF("system" , MKF(system_syscall  , "<system>" ), 1);
    DEF("sleep"  , MKF(sleep_syscall   , "<sleep>"  ), 1);
    DEF("time"   , MKB(time_builtin    , "<#time>"  ), 1);
    DEF("safe-fprintf", MKF(safe_fprintf_syscall, "<safe-fprintf>"), 1);
    DEF("pretty-print", MKF(pretty_print_syscall, "<pretty-print>"), 1);
}
