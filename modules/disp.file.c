
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

// ======================== 文件操作 ========================

// (fopen filename mode) -> file 或 nil
static disp_val* fopen_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_STRING || T(args[1]) != DISP_STRING) {
        ERET(NIL, "fopen: expects (filename mode)");
    }
    const char *filename = disp_get_str(args[0]);
    const char *mode = disp_get_str(args[1]);
    FILE *f = fopen(filename, mode);
    if (!f) {
        perror("fopen");
        return NIL;
    }
    // 注意：mode 字符串需要持久存储，因为 disp_make_file 只保存指针
    // 简单起见，复制一份
    char *mode_copy = gc_strdup(mode);
    if (!mode_copy) {
        fclose(f);
        return NIL;
    }
    return disp_make_file(f, mode_copy);
}

// (fclose file) -> bool
static disp_val* fclose_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_FILE) {
        ERET(NIL, "fclose: expects a file object");
    }
    FILE *f = disp_get_file(args[0]);
    // 如果文件指针已经是 NULL，说明已关闭，直接返回 TRUE
    if (f == NULL) return TRUE;
    if (f == stdin || f == stdout || f == stderr) {
        ERRO("fclose: cannot close standard stream");
        return NIL;
    }
    int ret = fclose(f);
    char *mode = disp_get_file_mode(args[0]);
    if (mode) { gc_free(mode); }
    // 将文件对象中的指针置空，防止重复关闭
    disp_set_file(args[0], NULL);
    disp_set_file_mode(args[0], NULL);
    if (ret == 0) return TRUE;
    else return NIL;
}

// (fread file) -> string 或 nil (读取一行)
static disp_val* fread_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_FILE) {
        ERET(NIL, "fread: expects a file object");
    }
    FILE *f = disp_get_file(args[0]);
    if (!f) return NIL;
    char buf[4096];
    if (fgets(buf, sizeof(buf), f) == NULL) {
        return NIL;   // EOF 或错误
    }
    // 去掉末尾换行符（可选）
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
    return disp_make_string(buf);
}

// (fwrite file string) -> bool
static disp_val* fwrite_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_FILE || T(args[1]) != DISP_STRING) {
        ERET(NIL, "fwrite: expects (file string)");
    }
    FILE *f = disp_get_file(args[0]);
    if (!f) return NIL;
    const char *str = disp_get_str(args[1]);
    int ret = fprintf(f, "%s", str);
    if (ret < 0) return NIL;
    return TRUE;
}

// (feof file) -> bool
static disp_val* feof_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_FILE) {
        ERET(NIL, "feof: expects a file object");
    }
    FILE *f = disp_get_file(args[0]);
    if (!f) return TRUE;  // 已关闭视为 EOF
    return feof(f) ? TRUE : NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("fopen" , MKF(fopen_syscall , "<fopen>" ), 1);
    DEF("fclose", MKF(fclose_syscall, "<fclose>"), 1);
    DEF("fread" , MKF(fread_syscall , "<fread>" ), 1);
    DEF("fwrite", MKF(fwrite_syscall, "<fwrite>"), 1);
    DEF("feof"  , MKF(feof_syscall  , "<feof>"  ), 1);
}
