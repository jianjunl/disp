
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"
#include "bestline/bestline.h"

/* ======================== REPL ======================== */
void disp_repl0() {

    printf("disp Lisp (type :quit to exit)\n");

    for (;;) {
        disp_info_t *_info = disp_get_current_info();
        if (_info && _info->filename) {
            printf("disp:");
            printf("%d", _info->line);
        } else printf("disp:");
        printf("> ");
        fflush(stdout);
/*
char *line = NULL;
size_t len = 0;
ssize_t nread = getline(&line, &len, stdin);
if (nread == -1) break;
fprintf(stderr, "RAW LINE: '%s' (len=%zu)\n", line, strlen(line));

FILE *mem = fmemopen(line, nread, "r");   // nread 包含换行符
if (!mem) {
    gc_free(line);
    break;
}

disp_val *expr = disp_read(mem);
fclose(mem);
gc_free(line);
*/
//sigset_t mask;
//sigemptyset(&mask);
//sigaddset(&mask, SIGUSR1);

//pthread_sigmask(SIG_BLOCK, &mask, NULL);
        disp_val *expr = disp_read(stdin);
//pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        if (!expr) break;
        disp_val *result = disp_eval(expr);
        disp_print(result);
        printf("\n");
        static int gc_counter = 0;
        if (++gc_counter > 1000) {
//            disp_gc();
            gc_counter = 0;
        }
    }
}


void disp_repl() {
    printf("disp Lisp (type :quit to exit)\n");

    const char *keywords[] = {
        "define", "lambda", "if", "cond", "else",
        "let", "let*", "letrec", "set!", "begin",
        "quote", "quasiquote", "unquote", "unquote-splicing",
        "and", "or", "not",
        "car", "cdr", "cons", "list", "append",
        "+", "-", "*", "/", "=", "<", ">",
        "gc", "load", "info", "trace", "quit"
    };
    bestlineSetHighlightKeywords(keywords, sizeof(keywords)/sizeof(keywords[0]));

    int repl_line = 1;   // REPL 起始行号

    for (;;) {
        // 动态提示符
        char prompt[256];
        disp_info_t *info = disp_get_current_info();
        if (info && info->filename) {
            snprintf(prompt, sizeof(prompt), "disp:%d> ", repl_line);
        } else {
            snprintf(prompt, sizeof(prompt), "disp> ");
        }

        char *line = bestline(prompt);
        if (!line) break;               // EOF or error
        if (strlen(line) == 0) {
            free(line);
            continue;
        }

        // ★ 把输入行加入历史，这样上下箭头就能用了
        bestlineHistoryAdd(line);

        // ★ 让解析器认为这一行从 repl_line 行开始
        parse_current_line = repl_line;

        // 使用 fmemopen 将字符串转换为 FILE* 供 disp_read 使用
        FILE *mem = fmemopen(line, strlen(line), "r");
        if (!mem) {
            free(line);
            continue;
        }

        disp_val *expr = disp_read(mem);
        fclose(mem);
        free(line);                     // 尽早释放 line

        if (!expr) {                    // parse error, skip
            repl_line++;                // 即使解析出错，行号也应增加
            continue;
        }

        disp_val *result = disp_eval(expr);
        disp_print(result);
        printf("\n");

        repl_line++;   // 读完一行后行号加 1
    }
}
