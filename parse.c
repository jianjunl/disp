
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

/* ======================== Parser ======================== */

extern int skip_and_get(FILE *f);

disp_val parse_sexpr(int first, FILE *f);

// parse_list 实现
static disp_val parse_list(FILE *f, char e, int comma_separated) {
    int c;
    disp_val head = NIL;       // 当前子列表的头
    disp_val tail = NIL;       // 当前子列表的尾
    disp_val sub_lists = NIL;  // 累积的子列表（用于逗号分隔）
    disp_val sub_tail = NIL;

    while (1) {
        c = skip_and_get(f);
        if (c == e) break;          // 正常结束
        if (c == EOF) {
            ERET(NIL, "unexpected EOF while reading list");
        }

        // 解析一个元素
        disp_val elem = parse_sexpr(c, f);
        if (N(elem)) return NIL;

        // 仅在非逗号分隔模式下处理点对
        if (!comma_separated && NN(elem) && T(elem) == FLAG_SYMBOL &&
            strcmp(SYM_NAME(elem), ".") == 0) {
            // 点对语法：下一个 sexpr 是 cdr
            int next_c = skip_and_get(f);
            if (next_c == EOF) {
                ERET(NIL, "unexpected EOF after '.'");
            }
            disp_val cdr_val = parse_sexpr(next_c, f);
            if (N(cdr_val)) return NIL;
            // 确保列表以 e 结束
            int close = skip_and_get(f);
            if (close != e) {
                ERET(NIL, "expected '%c' after dotted cdr", e);
            }
            if (E(head, NIL)) {
                ERET(NIL, "invalid dotted list: no car before '.'");
            }
            disp_set_cdr(tail, cdr_val);
            return head;   // 点对结束，不再继续
        }

        // 将元素添加到当前子列表
        disp_val new_cons = disp_make_cons(elem, NIL);
        if (E(head, NIL)) {
            head = new_cons;
            tail = new_cons;
        } else {
            disp_set_cdr(tail, new_cons);
            tail = new_cons;
        }

        if (comma_separated) {
            // 查看下一个字符是否为逗号
            int next_c = skip_and_get(f);
            if (next_c == ',') {
                // 遇到逗号：结束当前子列表，将其加入累积列表
                disp_val sub_cons = disp_make_cons(head, NIL);
                if (E(sub_lists, NIL)) {
                    sub_lists = sub_cons;
                    sub_tail = sub_cons;
                } else {
                    disp_set_cdr(sub_tail, sub_cons);
                    sub_tail = sub_cons;
                }
                // 重置当前子列表
                head = tail = NIL;
                continue;   // 继续解析下一个子列表
            } else {
                // 不是逗号，放回字符
                ungetc(next_c, f);
            }
        }
    }

    if (comma_separated) {
        // 将最后一个子列表加入累积列表（如果有元素或整个输入非空）
        if (NE(head, NIL) || E(sub_lists, NIL)) {
            disp_val sub_cons = disp_make_cons(head, NIL);
            if (E(sub_lists, NIL)) {
                sub_lists = sub_cons;
            } else {
                disp_set_cdr(sub_tail, sub_cons);
            }
        }
        return sub_lists;
    } else {
        // 非逗号分隔模式，直接返回单一列表
        return head;
    }
}

static disp_val parse_string(FILE *f) {
    char buf[1024];
    int i = 0, c;
    while ((c = fgetc(f)) != '"' && c != EOF) {
        if (c == '\\') {
            // 处理转义序列
            c = fgetc(f);
            if (c == EOF) break;
            switch (c) {
                case 'n':  c = '\n'; break;
                case 't':  c = '\t'; break;
                case 'r':  c = '\r'; break;
                case '\\': c = '\\'; break;
                case '"':  c = '"';  break;
                default:   /* 未知转义保留原字符（例如 \x） */ break;
            }
        }
        if (i < 1023) buf[i++] = c;
    }
    buf[i] = '\0';
    return disp_make_str(buf);
}

static disp_val parse_atom(int first, FILE *f) {
    char buf[256];
    int i = 0;
    buf[i++] = first;
    int c;
    while ((c = fgetc(f)) != EOF && !isspace(c) \
        && c != '(' && c != ')' \
        && c != '{' && c != '}' \
        && c != '[' && c != ']' \
    ) {
        if (i < 255) buf[i++] = c;
    }
    if (c != EOF) ungetc(c, f);
    buf[i] = '\0';
    disp_val num = disp_parse_number(buf);
    if (NN(num)) return num;

    // 处理 #t 和 #f 布尔常量
    if (buf[0] == '#') {
        if (strcmp(buf, "#t") == 0) return TRUE;
        if (strcmp(buf, "#f") == 0) return NIL;
        // 其他以 # 开头的字符串继续作为普通符号
    }
    disp_val sym = disp_intern_symbol_by_name(disp_global_env, buf);
    return sym;
}

disp_val parse_sexpr(int first, FILE *f) {
    if (first == '(') {
        return parse_list(f, ')', 0);
    } else if (first == '{') {
        // 保持不变：逗号分隔的 begin 语法
        disp_val sym = GLOBAL(BEGIN);
        if(N(sym) || E(sym, NIL)) ERET(NIL, "'begin' not found");
        disp_val sub_lists = parse_list(f, '}', 1);
        disp_val result = disp_make_cons(sym, NIL);
        disp_val tail = result;
        for (disp_val p = sub_lists; NN(p) && T(p) == FLAG_CONS; p = disp_cdr(p)) {
            disp_val sublist = disp_car(p);
            disp_val new_cons = disp_make_cons(sublist, NIL);
            disp_set_cdr(tail, new_cons);
            tail = new_cons;
        }
        return result;
    } else if (first == '[') {
        disp_val sym = GLOBAL(LIST);
        if(N(sym) || E(sym, NIL)) ERET(NIL, "'list' not found");
        return disp_make_cons(sym, parse_list(f, ']', 0));
    } else if (first == '"') {
        return parse_string(f);
    } else if (first == '\'') {
        // 单引号 -> quote
        int next_c = skip_and_get(f);
        if (next_c == EOF) ERET(NIL, "unexpected EOF after quote");
        disp_val quoted = parse_sexpr(next_c, f);
        if (N(quoted)) return NIL;
        disp_val quote_sym = GLOBAL(QUOTE);
        return disp_make_cons(quote_sym, disp_make_cons(quoted, NIL));
    } else if (first == '`') {
        // 反引号 -> quasiquote
        int next_c = skip_and_get(f);
        if (next_c == EOF) ERET(NIL, "unexpected EOF after quasiquote");
        disp_val quasiquoted = parse_sexpr(next_c, f);
        if (N(quasiquoted)) return NIL;
        disp_val qq_sym = GLOBAL(QUASIQUOTE);
        return disp_make_cons(qq_sym, disp_make_cons(quasiquoted, NIL));
    } else if (first == ',') {
        // 逗号 -> unquote 或 unquote-splicing
        int next_c = skip_and_get(f);
        if (next_c == EOF) ERET(NIL, "unexpected EOF after comma");
        if (next_c == '@') {
            // ,@ -> unquote-splicing
            int expr_c = skip_and_get(f);
            if (expr_c == EOF) ERET(NIL, "unexpected EOF after ,@");
            disp_val spliced = parse_sexpr(expr_c, f);
            if (N(spliced)) return NIL;
            disp_val us_sym = GLOBAL(UNQUOTE_SPLICING);
            return disp_make_cons(us_sym, disp_make_cons(spliced, NIL));
        } else {
            // , -> unquote
            disp_val unquoted = parse_sexpr(next_c, f);
            if (N(unquoted)) return NIL;
            disp_val uq_sym = GLOBAL(UNQUOTE);
            return disp_make_cons(uq_sym, disp_make_cons(unquoted, NIL));
        }
    } else if (first == ')') {
        ERET(DNULL, "unexpected ')'");
    } else if (first == '}') {
        ERET(DNULL, "unexpected '}'");
    } else if (first == ']') {
        ERET(DNULL, "unexpected ']'");
    } else {
        return parse_atom(first, f);
    }
}
