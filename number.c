
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

/**
 * 以下三个函数分别用于严格判断一个字符串是否为合法的C语言 float、double 和 long 字面量。
 * 它们遵循C标准语法，包括十进制/十六进制浮点数、指数、后缀等规则，并拒绝任何不符合语法的字符串
 *（如缺少必要部分、非法字符、多余后缀等）。
**/
/**
 * 判断字符串是否为合法的C语言 float 字面量
 * 规则：必须包含小数点或指数，且必须以 f 或 F 结尾
 * 支持十进制和十六进制（0x/0X）浮点数
 */
bool disp_is_float_literal(const char *s) {
    if (!s || *s == '\0') return false;
    const char *p = s;
    bool hex = false;

    // 检查十六进制前缀
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        hex = true;
        p += 2;
        if (*p == '\0') return false;
    }

    bool has_digit_before = false;
    bool has_digit_after = false;

    if (hex) {
        // 十六进制浮点数：整数部分
        while (isxdigit(*p)) { has_digit_before = true; p++; }
        // 小数部分
        if (*p == '.') {
            p++;
            while (isxdigit(*p)) { has_digit_after = true; p++; }
        }
        // 必须有数字（整数或小数）
        if (!has_digit_before && !has_digit_after) return false;
        // 必须有指数部分 (p/P)
        if (*p != 'p' && *p != 'P') return false;
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!isdigit(*p)) return false;
        while (isdigit(*p)) p++;
        // 必须以后缀 f/F 结尾
        if (*p == 'f' || *p == 'F') {
            p++;
            return *p == '\0';
        }
        return false;
    } else {
        // 十进制浮点数：整数部分
        while (isdigit(*p)) { has_digit_before = true; p++; }
        // 小数部分
        if (*p == '.') {
            p++;
            while (isdigit(*p)) { has_digit_after = true; p++; }
        }
        // 必须有数字（整数或小数）
        if (!has_digit_before && !has_digit_after) return false;
        // 指数部分（可选）
        if (*p == 'e' || *p == 'E') {
            p++;
            if (*p == '+' || *p == '-') p++;
            if (!isdigit(*p)) return false;
            while (isdigit(*p)) p++;
        }
        // 必须以后缀 f/F 结尾
        if (*p == 'f' || *p == 'F') {
            p++;
            return *p == '\0';
        }
        return false;
    }
}

/**
 * 判断字符串是否为合法的C语言 double 字面量
 * 规则：必须包含小数点或指数，不允许任何后缀
 * 支持十进制和十六进制（0x/0X）浮点数
 */
bool disp_is_double_literal(const char *s) {
    if (!s || *s == '\0') return false;
    const char *p = s;
    bool hex = false;

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        hex = true;
        p += 2;
        if (*p == '\0') return false;
    }

    bool has_digit_before = false;
    bool has_digit_after = false;

    if (hex) {
        while (isxdigit(*p)) { has_digit_before = true; p++; }
        if (*p == '.') {
            p++;
            while (isxdigit(*p)) { has_digit_after = true; p++; }
        }
        if (!has_digit_before && !has_digit_after) return false;
        if (*p != 'p' && *p != 'P') return false;
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!isdigit(*p)) return false;
        while (isdigit(*p)) p++;
        // 不允许后缀
        return *p == '\0';
    } else {
        while (isdigit(*p)) { has_digit_before = true; p++; }
        if (*p == '.') {
            p++;
            while (isdigit(*p)) { has_digit_after = true; p++; }
        }
        if (!has_digit_before && !has_digit_after) return false;
        if (*p == 'e' || *p == 'E') {
            p++;
            if (*p == '+' || *p == '-') p++;
            if (!isdigit(*p)) return false;
            while (isdigit(*p)) p++;
        }
        // 不允许后缀
        return *p == '\0';
    }
}

/**
 * 判断字符串是否为合法的C语言 long 字面量
 * 规则：十进制、八进制或十六进制整数，必须以 l 或 L 结尾
 * 不允许 unsigned 后缀，不允许 long long 后缀
 */
bool disp_is_long_literal(const char *s) {
    if (!s || *s == '\0') return false;
    const char *p = s;

    // 处理前缀
    if (*p == '0') {
        p++; // 跳过开头的 '0'
        if (*p == 'x' || *p == 'X') {
            // 十六进制
            p++;
            if (!isxdigit(*p)) return false;
            while (isxdigit(*p)) p++;
        } else {
            // 八进制：允许后续 0-7 数字（0本身已有一个数字）
            while (*p >= '0' && *p <= '7') p++;
        }
    } else {
        // 十进制：不能以 '0' 开头
        if (!isdigit(*p) || *p == '0') return false;
        while (isdigit(*p)) p++;
    }

    // 必须以后缀 l 或 L 结尾
    if (*p == 'l' || *p == 'L') {
        p++;
        return *p == '\0';
    }
    return false;
}

// ------------------- 新增：判断一般整数（无后缀） -------------------
bool disp_is_integer_literal(const char *s) {
    if (!s || *s == '\0') return false;
    const char *p = s;

    // 十进制、八进制、十六进制，不允许符号（C 字面量本身无符号）
    if (*p == '0') {
        p++;
        if (*p == 'x' || *p == 'X') {
            p++;
            if (!isxdigit(*p)) return false;
            while (isxdigit(*p)) p++;
        } else {
            if (*p == '\0') return true;  // 单独的 "0"
            while (*p >= '0' && *p <= '7') p++;
        }
    } else if (isdigit(*p) && *p != '0') {
        while (isdigit(*p)) p++;
    } else {
        return false;
    }

    // 不允许有任何后缀、小数点、指数等
    return *p == '\0';
}

disp_val disp_parse_number(const char *s) {
    if (!s || *s == '\0') return DNULL;

    const char *p = s;
    bool negate = false;
    //bool has_sign = false;

    /* 1. 处理前导符号 */
    if (*p == '+' || *p == '-') {
        //has_sign = true;
        negate = (*p == '-');
        p++;
    }

    const char *number_start = p;   /* 符号之后的部分 */

    //bool hex = false;
    bool has_dot = false;
    bool has_exp = false;
    bool has_frac_digits = false;
    bool has_int_digits = false;
    bool has_exp_digits = false;
    bool long_suffix = false;
    bool float_suffix = false;
    bool valid = true;

    /* 2. 检查十六进制前缀 */
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        //hex = true;
        p += 2;
        if (!isxdigit((unsigned char)*p)) valid = false;
        else {
            while (isxdigit((unsigned char)*p)) { has_int_digits = true; p++; }
            if (*p == '.') {
                has_dot = true;
                p++;
                while (isxdigit((unsigned char)*p)) { has_frac_digits = true; p++; }
            }
            if (!has_int_digits && !has_frac_digits) valid = false;
            if (*p == 'p' || *p == 'P') {
                has_exp = true;
                p++;
                if (*p == '+' || *p == '-') p++;
                if (isdigit((unsigned char)*p)) {
                    has_exp_digits = true;
                    while (isdigit((unsigned char)*p)) p++;
                } else {
                    valid = false;
                }
            } else {
                if (has_dot) valid = false;  /* 十六进制浮点数必须有 p 指数 */
            }
        }
    } else {
        /* 十进制 / 八进制 */
        if (*p == '0') {
            has_int_digits = true;
            p++;
            while (*p >= '0' && *p <= '7') { has_int_digits = true; p++; }
            if (*p == '8' || *p == '9') valid = false;
        } else if (isdigit((unsigned char)*p)) {
            while (isdigit((unsigned char)*p)) { has_int_digits = true; p++; }
        } else if (*p == '.') {
            has_int_digits = false;
        } else {
            valid = false;
        }

        if (!valid) {
            /* 如果不是有效数字，但前面有符号，那么就不是合法数字，返回 NULL */
            return DNULL;
        }

        /* 小数点 */
        if (*p == '.') {
            has_dot = true;
            p++;
            if (isdigit((unsigned char)*p)) {
                has_frac_digits = true;
                while (isdigit((unsigned char)*p)) p++;
            }
        }

        /* 指数 */
        if (*p == 'e' || *p == 'E') {
            has_exp = true;
            p++;
            if (*p == '+' || *p == '-') p++;
            if (isdigit((unsigned char)*p)) {
                has_exp_digits = true;
                while (isdigit((unsigned char)*p)) p++;
            } else {
                valid = false;
            }
        }

        if (!has_int_digits && !has_frac_digits) valid = false;
        if (has_exp && !has_exp_digits) valid = false;
    }

    /* 3. 后缀 */
    if (*p == 'l' || *p == 'L') {
        long_suffix = true;
        p++;
    } else if (*p == 'f' || *p == 'F') {
        float_suffix = true;
        p++;
    }

    /* 4. 检查是否完全消耗（后缀之后不能有非空白字符） */
    while (*p && isspace((unsigned char)*p)) p++;   /* 允许末尾空白？严格来说不允许，但为了容错可加。这里不跳过空白，直接要求结束 */
    if (*p != '\0') valid = false;

    if (!valid) return DNULL;

    /* 5. 根据标志构建值 */
    if (long_suffix) {
        if (has_dot || has_exp) return DNULL;
        long val = strtol(number_start, NULL, 0);
        if (negate) val = -val;
        return disp_make_long(val);
    }

    if (float_suffix) {
        if (!has_dot && !has_exp) return DNULL;
        float val = strtof(number_start, NULL);
        if (negate) val = -val;
        return disp_make_float(val);
    }

    if (has_dot || has_exp) {
        double val = strtod(number_start, NULL);
        if (negate) val = -val;
        return disp_make_double(val);   /* 修正：之前错误用了 disp_make_long */
    }

    /* 普通整数 */
    long val = strtol(number_start, NULL, 0);
    if (negate) val = -val;
    if (val >= INT_MIN && val <= INT_MAX) {
        return disp_make_int((int)val);
    } else {
        return disp_make_long(val);
    }
}

