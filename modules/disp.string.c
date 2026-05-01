
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#include <strings.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

/* 辅助函数：将任意 disp_val 转换为字符串（用于 number->string 和 string 拼接） */
static char* val_to_write_str(disp_val *v) {
    if (v == NIL) return gc_strdup("nil");
    if (v == TRUE) return gc_strdup("true");
    if (T(v) == DISP_STRING) return gc_strdup(disp_get_str(v));
    // 对于其他类型，使用 disp_str（不带外部引号的表示）
    return disp_str(v);
}

/* ----- make-string ----- */
static disp_val* make_string_syscall(disp_val **args, int count) {
    if (count < 1 || count > 2)
        ERET(NIL, "make-string: expects (length [fill])");
    if (T(args[0]) != DISP_INT)
        ERET(NIL, "make-string: length must be integer");
    int len = disp_get_int(args[0]);
    if (len < 0) ERET(NIL, "make-string: negative length");
    char fill = ' ';
    if (count == 2) {
        if (T(args[1]) != DISP_BYTE && T(args[1]) != DISP_STRING)
            ERET(NIL, "make-string: fill must be a character or string of length 1");
        // 简单处理：如果是字符串且长度为1，取第一个字符；否则默认空格
        if (T(args[1]) == DISP_STRING) {
            const char *s = disp_get_str(args[1]);
            if (strlen(s) == 1) fill = s[0];
        } else if (T(args[1]) == DISP_BYTE) {
            fill = disp_get_byte(args[1]);
        }
    }
    char *buf = gc_malloc(len + 1);
    if (!buf) return NIL;
    memset(buf, fill, len);
    buf[len] = '\0';
    disp_val *result = disp_make_string(buf);
    gc_free(buf);
    return result;
}

/* ----- string ----- 将任意参数转为字符串表示并拼接 */
static disp_val* string_syscall(disp_val **args, int count) {
    size_t total_len = 0;
    char **parts = gc_malloc(count * sizeof(char*));
    if (!parts) return NIL;
    for (int i = 0; i < count; i++) {
        parts[i] = val_to_write_str(args[i]);
        total_len += strlen(parts[i]);
    }
    char *buf = gc_malloc(total_len + 1);
    if (!buf) {
        for (int i = 0; i < count; i++) gc_free(parts[i]);
        gc_free(parts);
        return NIL;
    }
    buf[0] = '\0';
    for (int i = 0; i < count; i++) {
        strcat(buf, parts[i]);
        gc_free(parts[i]);
    }
    gc_free(parts);
    disp_val *result = disp_make_string(buf);
    gc_free(buf);
    return result;
}

/* ----- string-length ----- */
static disp_val* string_length_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_STRING)
        ERET(NIL, "string-length: expects a string");
    size_t len = disp_get_str_len(args[0]);
    return disp_make_int((int)len);
}

/* ----- string-ref ----- */
static disp_val* string_ref_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_STRING)
        ERET(NIL, "string-ref: expects (string index)");
    int idx = 0;
    if (T(args[1]) == DISP_INT)
        idx = disp_get_int(args[1]);
    else if (T(args[1]) == DISP_LONG)
        idx = (int)disp_get_long(args[1]);
    else
        ERET(NIL, "string-ref: index must be integer");
    const char *str = disp_get_str(args[0]);
    size_t len = strlen(str);
    if (idx < 0 || (size_t)idx >= len)
        ERET(NIL, "string-ref: index out of range");
    // 返回字符（作为单字符字符串或字符类型？目前没有 DISP_CHAR，我们用字符串）
    char buf[2] = { str[idx], '\0' };
    return disp_make_string(buf);
}

/* ----- string-set! (破坏性修改) ----- */
static disp_val* string_set_syscall(disp_val **args, int count) {
    if (count != 3 || T(args[0]) != DISP_STRING)
        ERET(NIL, "string-set!: expects (string index char)");
    int idx = 0;
    if (T(args[1]) == DISP_INT)
        idx = disp_get_int(args[1]);
    else if (T(args[1]) == DISP_LONG)
        idx = (int)disp_get_long(args[1]);
    else
        ERET(NIL, "string-set!: index must be integer");
    char c;
    if (T(args[2]) == DISP_STRING) {
        const char *s = disp_get_str(args[2]);
        if (strlen(s) != 1) ERET(NIL, "string-set!: char must be a single-character string");
        c = s[0];
    } else {
        ERET(NIL, "string-set!: char must be a string of length 1");
    }
    char *str = disp_get_str(args[0]);
    size_t len = strlen(str);
    if (idx < 0 || (size_t)idx >= len)
        ERET(NIL, "string-set!: index out of range");
    str[idx] = c;
    return args[0];
}

/* ----- substring ----- */
static disp_val* substring_syscall(disp_val **args, int count) {
    if (count != 3 || T(args[0]) != DISP_STRING)
        ERET(NIL, "substring: expects (string start end)");
    int start = 0, end = 0;
    if (T(args[1]) == DISP_INT) start = disp_get_int(args[1]);
    else if (T(args[1]) == DISP_LONG) start = (int)disp_get_long(args[1]);
    else ERET(NIL, "substring: start must be integer");
    if (T(args[2]) == DISP_INT) end = disp_get_int(args[2]);
    else if (T(args[2]) == DISP_LONG) end = (int)disp_get_long(args[2]);
    else ERET(NIL, "substring: end must be integer");
    const char *str = disp_get_str(args[0]);
    size_t len = strlen(str);
    if (start < 0 || end < start || (size_t)end > len)
        ERET(NIL, "substring: invalid range");
    size_t sub_len = end - start;
    char *buf = gc_malloc(sub_len + 1);
    if (!buf) return NIL;
    memcpy(buf, str + start, sub_len);
    buf[sub_len] = '\0';
    disp_val *result = disp_make_string(buf);
    gc_free(buf);
    return result;
}

/* ----- string-append ----- */
static disp_val* string_append_syscall(disp_val **args, int count) {
    size_t total_len = 0;
    char **parts = gc_malloc(count * sizeof(char*));
    if (!parts) return NIL;
    for (int i = 0; i < count; i++) {
        if (T(args[i]) != DISP_STRING) {
            gc_free(parts);
            ERET(NIL, "string-append: all arguments must be strings");
        }
        parts[i] = disp_get_str(args[i]);
        total_len += strlen(parts[i]);
    }
    char *buf = gc_malloc(total_len + 1);
    if (!buf) {
        gc_free(parts);
        return NIL;
    }
    buf[0] = '\0';
    for (int i = 0; i < count; i++) {
        strcat(buf, parts[i]);
    }
    gc_free(parts);
    disp_val *result = disp_make_string(buf);
    gc_free(buf);
    return result;
}

/* ----- string->list ----- */
static disp_val* string_to_list_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_STRING)
        ERET(NIL, "string->list: expects a string");
    const char *str = disp_get_str(args[0]);
    disp_val *list = NIL;
    size_t len = strlen(str);
    for (size_t i = len; i-- > 0; ) {
        char buf[2] = { str[i], '\0' };
        disp_val *ch = disp_make_string(buf);
        list = disp_make_cons(ch, list);
    }
    return list;
}

/* ----- list->string ----- */
static disp_val* list_to_string_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_CONS)
        ERET(NIL, "list->string: expects a list of characters (single-char strings)");
    // 计算长度并收集字符
    size_t len = 0;
    for (disp_val *p = args[0]; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        disp_val *item = disp_car(p);
        if (T(item) != DISP_STRING) {
            ERET(NIL, "list->string: list must contain only strings");
        }
        if (strlen(disp_get_str(item)) != 1) {
            ERET(NIL, "list->string: each element must be a single-character string");
        }
        len++;
    }
    char *buf = gc_malloc(len + 1);
    if (!buf) return NIL;
    size_t i = 0;
    for (disp_val *p = args[0]; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        disp_val *item = disp_car(p);
        buf[i++] = disp_get_str(item)[0];
    }
    buf[i] = '\0';
    disp_val *result = disp_make_string(buf);
    gc_free(buf);
    return result;
}

/* ----- string-copy ----- */
static disp_val* string_copy_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_STRING)
        ERET(NIL, "string-copy: expects a string");
    return disp_make_string(disp_get_str(args[0]));
}

/* ----- 字符串比较 ----- */
#pragma GCC diagnostic ignored "-Wunused-function"
static int string_compare(disp_val *a, disp_val *b, int case_sensitive) {
    const char *sa = disp_get_str(a);
    const char *sb = disp_get_str(b);
    if (case_sensitive) return strcmp(sa, sb);
    else return strcasecmp(sa, sb);
}

static disp_val* string_eq_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_STRING || T(args[1]) != DISP_STRING)
        ERET(NIL, "string=? expects two strings");
    return (strcmp(disp_get_str(args[0]), disp_get_str(args[1])) == 0) ? TRUE : NIL;
}

static disp_val* string_ci_eq_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_STRING || T(args[1]) != DISP_STRING)
        ERET(NIL, "string-ci=? expects two strings");
    return (strcasecmp(disp_get_str(args[0]), disp_get_str(args[1])) == 0) ? TRUE : NIL;
}

static disp_val* string_lt_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_STRING || T(args[1]) != DISP_STRING)
        ERET(NIL, "string<? expects two strings");
    return (strcmp(disp_get_str(args[0]), disp_get_str(args[1])) < 0) ? TRUE : NIL;
}

static disp_val* string_gt_syscall(disp_val **args, int count) {
    if (count != 2 || T(args[0]) != DISP_STRING || T(args[1]) != DISP_STRING)
        ERET(NIL, "string>? expects two strings");
    return (strcmp(disp_get_str(args[0]), disp_get_str(args[1])) > 0) ? TRUE : NIL;
}

/* ----- 大小写转换 ----- */
static disp_val* string_upcase_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_STRING)
        ERET(NIL, "string-upcase: expects a string");
    const char *s = disp_get_str(args[0]);
    char *buf = gc_strdup(s);
    if (!buf) return NIL;
    for (char *p = buf; *p; p++) *p = toupper((unsigned char)*p);
    disp_val *result = disp_make_string(buf);
    gc_free(buf);
    return result;
}

static disp_val* string_downcase_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_STRING)
        ERET(NIL, "string-downcase: expects a string");
    const char *s = disp_get_str(args[0]);
    char *buf = gc_strdup(s);
    if (!buf) return NIL;
    for (char *p = buf; *p; p++) *p = tolower((unsigned char)*p);
    disp_val *result = disp_make_string(buf);
    gc_free(buf);
    return result;
}

/* ----- 去除空白字符 ----- */
static disp_val* string_trim_syscall(disp_val **args, int count) {
    if (count < 1 || count > 2 || T(args[0]) != DISP_STRING)
        ERET(NIL, "string-trim: expects (string [char-set])");
    const char *s = disp_get_str(args[0]);
    const char *trim_chars = " \t\n\r\f\v";
    if (count == 2 && T(args[1]) == DISP_STRING) {
        trim_chars = disp_get_str(args[1]);
    }
    // 左端
    const char *start = s;
    while (*start && strchr(trim_chars, *start)) start++;
    // 右端
    const char *end = s + strlen(s) - 1;
    while (end >= start && strchr(trim_chars, *end)) end--;
    size_t new_len = (end >= start) ? (end - start + 1) : 0;
    char *buf = gc_malloc(new_len + 1);
    if (!buf) return NIL;
    memcpy(buf, start, new_len);
    buf[new_len] = '\0';
    disp_val *result = disp_make_string(buf);
    gc_free(buf);
    return result;
}

/* ----- number->string ----- */
static disp_val* number_to_string_syscall(disp_val **args, int count) {
    if (count != 1) ERET(NIL, "number->string: expects one number");
    char *s = disp_str(args[0]);   // disp_str 返回可读表示
    if (!s) return NIL;
    disp_val *result = disp_make_string(s);
    gc_free(s);
    return result;
}

/* ----- string->number ----- */
static disp_val* string_to_number_syscall(disp_val **args, int count) {
    if (count != 1 || T(args[0]) != DISP_STRING)
        ERET(NIL, "string->number: expects a string");
    const char *s = disp_get_str(args[0]);
    disp_val *num = disp_parse_number(s);
    if (num) return num;
    return NIL;
}

/* ----- 模块初始化 ----- */
void disp_init_module(void) {
    DEF("make-string"      , MKF(make_string_syscall      , "<make-string>"), 1);
    DEF("string"           , MKF(string_syscall           , "<string>")  , 1);
    DEF("string-length"    , MKF(string_length_syscall    , "<string-length>"), 1);
    DEF("string-ref"       , MKF(string_ref_syscall       , "<string-ref>"), 1);
    DEF("string-set!"      , MKF(string_set_syscall       , "<string-set!>"), 1);
    DEF("substring"        , MKF(substring_syscall        , "<substring>"), 1);
    DEF("string-append"    , MKF(string_append_syscall    , "<string-append>"), 1);
    DEF("string->list"     , MKF(string_to_list_syscall   , "<string->list>"), 1);
    DEF("list->string"     , MKF(list_to_string_syscall   , "<list->string>"), 1);
    DEF("string-copy"      , MKF(string_copy_syscall      , "<string-copy>"), 1);
    DEF("string=?"         , MKF(string_eq_syscall        , "<string=?>"), 1);
    DEF("string-ci=?"      , MKF(string_ci_eq_syscall     , "<string-ci=?>"), 1);
    DEF("string<?"         , MKF(string_lt_syscall        , "<string<?>"), 1);
    DEF("string>?"         , MKF(string_gt_syscall        , "<string>?>"), 1);
    DEF("string-upcase"    , MKF(string_upcase_syscall    , "<string-upcase>"), 1);
    DEF("string-downcase"  , MKF(string_downcase_syscall  , "<string-downcase>"), 1);
    DEF("string-trim"      , MKF(string_trim_syscall      , "<string-trim>"), 1);
    DEF("number->string"   , MKF(number_to_string_syscall , "<number->string>"), 1);
    DEF("string->number"   , MKF(string_to_number_syscall , "<string->number>"), 1);
}
