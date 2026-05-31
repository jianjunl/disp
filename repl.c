
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

/* ANSI 颜色定义 */
#define CLR_KEYWORD   "\033[1;31m"   // 红色粗体
#define CLR_STRING    "\033[0;32m"   // 绿色
#define CLR_NUMBER    "\033[0;33m"   // 黄色
#define CLR_COMMENT   "\033[0;36m"   // 青色
#define CLR_RESET     "\033[0m"

/* ======================== REPL ======================== */

void disp_repl2() {

    printf("disp Lisp (type :quit to exit)\n");

    for (;;) {
        disp_info_t *_info = disp_get_current_info();
        if (_info && _info->filename) {
            printf("disp:");
            printf("%d", _info->line);
        } else printf("disp:");
        printf("> ");
        fflush(stdout);

        disp_val expr = disp_read(stdin);
        if (N(expr)) break;
        disp_val result = disp_eval(disp_global_env, expr);
        disp_print(result);
        printf("\n");
        static int gc_counter = 0;
        if (++gc_counter > 1000) {
//            disp_gc();
            gc_counter = 0;
        }
    }
}

static int repl_started = 0;
extern int parse_current_line;

void disp_repl() {
    if (repl_started) {
        ERRO("disp REPL has been started");
        return;
    }
    printf("disp Lisp (type :quit to exit)\n");

    #include "keywords.h"

    bestlineBalanceMode(1);
    bestlineSetHighlightKeywords(keywords, sizeof(keywords)/sizeof(keywords[0]));

    int repl_line = 1;
    disp_val args = disp_eval(disp_global_env, ARGS);
    int argc = disp_get_argc(args);
    char **argv =  disp_get_argv(args);
    for (int i = 1; i < argc; i++) {
        disp_import(argv[i]);
    }
    for (;;) {
        char prompt[256];
        disp_info_t *info = disp_get_current_info();
        if (info && info->filename)
            snprintf(prompt, sizeof(prompt), "disp:%d> ", repl_line);
        else
            snprintf(prompt, sizeof(prompt), "disp> ");

        char *line = bestlineWithHistory(prompt, "disp");
        if (!line) break;
        if (strlen(line) == 0) { free(line); continue; }

        parse_current_line = repl_line;
        FILE *mem = fmemopen(line, strlen(line), "r");
        if (!mem) { free(line); continue; }

        repl_started = 1;
        disp_val expr = disp_read(mem);

        if (NN(expr) && T(expr) == FLAG_SYMBOL && strcmp(SN(expr), ":clh") == 0) {
            bestlineHistoryFree();
            printf("History cleared.\n");
            free(line);
            repl_line++;
            continue;
        }
        fclose(mem);

        if (N(expr)) {
            free(line);
            repl_line++;
            continue;
        }

        disp_val result = disp_eval(disp_global_env, expr);
        disp_print(result);
        printf("\n");

        free(line);
        repl_line++;
    }
}
