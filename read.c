
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

/* ======================== Reader ======================== */

extern void disp_update_pos(int c);

int skip_and_get(FILE *f) {
    int c, d;
    while (1) {
        //c = fgetc(f);
        //if (c == EOF) return EOF;
        c = fgetc(f);
        if (c == EOF) return EOF;
        disp_update_pos(c);      // 更新行列号，包括空白字符
        if (c == ';') {
            // 单行注释：一直读到换行
            while ((c = fgetc(f)) != '\n' && c != EOF) {
                if (c == EOF) return EOF;
                disp_update_pos(c);
            }
            if (c == '\n') disp_update_pos(c);
            continue;
        }

        if (c == '#') {
            d = fgetc(f);
            if (d == EOF) return EOF;
            disp_update_pos(d);
            if (d == '|') {
                // 多行注释，支持嵌套
                int depth = 1;
                while (depth > 0) {
                    c = fgetc(f);
                    if (c == EOF) {
                        ERET(EOF, "unexpected EOF inside multi-line comment");
                    }
                    disp_update_pos(c);
                    if (c == '#') {
                        d = fgetc(f);
                        if (d == EOF) return EOF;
                        disp_update_pos(d);
                        if (d == '|') depth++;
                        else ungetc(d, f);
                    } else if (c == '|') {
                        d = fgetc(f);
                        if (d == EOF) return EOF;
                        disp_update_pos(d);
                        if (d == '#') depth--;
                        else ungetc(d, f);
                    }
                }
                continue;
            } else if (d == 't' || d == 'f') {
                // #t 或 #f 不作为注释，将 d 放回，返回 '#'
                ungetc(d, f);
                return c;   // 返回 '#'
            } else {
                // 单行注释（# ... 换行）
                ungetc(d, f);
                while ((c = fgetc(f)) != '\n' && c != EOF) {
                    if (c == EOF) return EOF;
                    disp_update_pos(c);
                }
                if (c == '\n') disp_update_pos(c);
                continue;
            }
        }

        if (!isspace(c)) {
            // 返回非空白字符，该字符位置已经在 disp_update_pos 中记录
            return c;
        }
    }
}

extern disp_val parse_sexpr(int first, FILE *f);

extern int parse_current_line;
extern int parse_current_col;
extern void disp_update_current_pos(int line, int col); // 更新栈顶位置

disp_val disp_read(FILE *f) {
    int c = skip_and_get(f);
    if (c == EOF) return DNULL;
    // 记录当前表达式开始的位置（此时行列号已指向该字符的下一个位置）
    int start_line = parse_current_line;
    int start_col = parse_current_col - 1;
    if (start_col == 0) { start_line--; start_col = 1; }
    disp_val expr = parse_sexpr(c, f);
    if (NN(expr)) {
        disp_update_current_pos(start_line, start_col);
    }
    return expr;
}
