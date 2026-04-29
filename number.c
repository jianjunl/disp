
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

disp_val* disp_parse_number(const char *s) {
    if (!s || *s == '\0') {
        return NULL;
    }

    const char *p = s;
    bool hex = false;
    bool has_dot = false;
    bool has_exp = false;
    bool has_frac_digits = false;   // 小数部分是否有数字
    bool has_int_digits = false;     // 整数部分是否有数字
    bool has_exp_digits = false;     // 指数部分是否有数字
    bool long_suffix = false;
    bool float_suffix = false;
    bool valid = true;

    // 1. 检查十六进制前缀
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        hex = true;
        p += 2;
        if (*p == '\0') valid = false; // 只有 0x 非法
    }

    // 2. 解析数字部分
    if (hex) {
        // 十六进制整数部分
        while (isxdigit(*p)) {
            has_int_digits = true;
            p++;
        }
        // 小数点
        if (*p == '.') {
            has_dot = true;
            p++;
            while (isxdigit(*p)) {
                has_frac_digits = true;
                p++;
            }
        }
        // 必须有数字（整数或小数）
        if (!has_int_digits && !has_frac_digits) valid = false;
        // 十六进制浮点数必须有指数部分 p/P
        if (*p == 'p' || *p == 'P') {
            has_exp = true;
            p++;
            if (*p == '+' || *p == '-') p++;
            if (isdigit(*p)) {
                has_exp_digits = true;
                while (isdigit(*p)) p++;
            } else {
                valid = false;
            }
        } else {
            // 没有指数部分，则只能是整数（十六进制整数不允许有小数点）
            if (has_dot) valid = false;
            // 没有指数，也没有小数点，合法十六进制整数
        }
    } else {
        // 十进制（可能八进制，但八进制只能出现在整数且没有小数点/指数）
        // 先解析整数部分
        if (*p == '0') {
            // 可能是八进制或单纯的0
            has_int_digits = true;
            p++;
            // 八进制数字（0-7）
            while (*p >= '0' && *p <= '7') {
                has_int_digits = true;
                p++;
            }
            // 如果后面紧跟 8 或 9，则不是合法的八进制，但可能构成十进制浮点数？例如 "09.0" 是非法的，因为八进制不能有 9
            // C标准：以0开头的数字序列，若包含8或9，则整个常量非法（除非后面有小数点或指数，但八进制浮点数不存在）
            // 为简单起见，这里统一处理：如果整数部分以0开头且后面跟着8或9，则视为非法（因为不能作为八进制，也不能作为十进制）
            if (*p == '8' || *p == '9') {
                valid = false;
            }
        } else if (isdigit(*p)) {
            // 十进制，不能以0开头
            while (isdigit(*p)) {
                has_int_digits = true;
                p++;
            }
        } else {
            // 不是数字开头，可能是小数点开头？C语言浮点字面量允许 .123 形式
            // 此时整数部分没有数字
            has_int_digits = false;
        }

        // 小数点
        if (*p == '.') {
            has_dot = true;
            p++;
            while (isdigit(*p)) {
                has_frac_digits = true;
                p++;
            }
        }

        // 指数部分
        if (*p == 'e' || *p == 'E') {
            has_exp = true;
            p++;
            if (*p == '+' || *p == '-') p++;
            if (isdigit(*p)) {
                has_exp_digits = true;
                while (isdigit(*p)) p++;
            } else {
                valid = false;
            }
        }

        // 整数部分和小数部分必须至少有一个数字
        if (!has_int_digits && !has_frac_digits) valid = false;
        // 如果有指数部分，必须要有指数数字
        if (has_exp && !has_exp_digits) valid = false;
    }

    // 3. 解析后缀
    if (*p == 'l' || *p == 'L') {
        long_suffix = true;
        p++;
    } else if (*p == 'f' || *p == 'F') {
        float_suffix = true;
        p++;
    }

    // 4. 检查是否完全消耗字符串
    if (*p != '\0') valid = false;

    // 5. 根据标志判断类型
    if (!valid) {
        return NULL;
    }

    // long literal: 必须有 long 后缀，且不能有小数点或指数
    if (long_suffix) {
        if (has_dot || has_exp) {
            // 非法：后缀 l/L 只能用于整数
            return NULL;
        }
        long val = strtol(s, NULL, 0);
        return disp_make_long(val);
    }

    // float literal: 必须有 f/F 后缀，且必须有小数点或指数
    if (float_suffix) {
        if (!has_dot && !has_exp) {
            // 非法：f/F 后缀必须用于浮点数
            return NULL;
        }
        float val = strtof(s, NULL);
        return disp_make_float(val);
    }

    // double literal: 无后缀，但包含小数点或指数
    if (has_dot || has_exp) {
        double val = strtod(s, NULL);
        return disp_make_long(val);
    }

    // 一般整数: 无后缀，无小数点，无指数
    if (!has_dot && !has_exp) {
        // 一般整数（int），使用 strtol 转换后转为 int
        long val = strtol(s, NULL, 0);
        // 无后缀，根据范围决定 int 或 long
        if (val >= INT_MIN && val <= INT_MAX) {
            return disp_make_int((int)val);
        } else {
            return disp_make_long(val);
        }
    }

    // 理论上不会走到这里，但为了安全
    return NULL;
}
