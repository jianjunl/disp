
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

void disp_repl() {
    printf("disp Lisp (type :quit to exit)\n");

    #include "keywords.h"

    bestlineBalanceMode(1);
    bestlineSetHighlightKeywords(keywords, sizeof(keywords)/sizeof(keywords[0]));

    int repl_line = 1;
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

        disp_val *expr = disp_read(mem);
        if (expr && T(expr) == DISP_SYMBOL && strcmp(S(expr), ":clh") == 0) {
            bestlineHistoryFree();
            printf("History cleared.\n");
            free(line);
            repl_line++;
            continue;
        }
        fclose(mem);

        if (!expr) {
            free(line);
            repl_line++;
            continue;
        }

        disp_val *result = disp_eval(expr);
        disp_print(result);
        printf("\n");

        free(line);
        repl_line++;
    }
}
