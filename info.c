
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
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

_Thread_local disp_info_t *current_info = NULL;

void disp_push_source(const char *filename) {
    disp_info_t *new_info = gc_calloc(1, sizeof(disp_info_t));
    new_info->filename = filename ? gc_strdup(filename) : NULL;
    new_info->line = 1;
    new_info->column = 1;
    // 保存当前解析器的行列号
    new_info->saved_line = parse_current_line;
    new_info->saved_col = parse_current_col;
    new_info->next = current_info;
    current_info = new_info;
    // 重置解析器行列号为新文件的第一行第一列
    parse_current_line = 1;
    parse_current_col = 1;
}

void disp_pop_source(void) {
    if (current_info) {
        disp_info_t *old = current_info;
        current_info = current_info->next;
        // 恢复解析器的行列号
        parse_current_line = old->saved_line;
        parse_current_col = old->saved_col;
        gc_free(old->filename);
        gc_free(old);
    }
}

void disp_init_info(void) {
    gc_add_root(&current_info);
}

void disp_update_current_pos(int line, int col) {
    if (current_info) {
        current_info->line = line;
        current_info->column = col;
    }
}

disp_info_t* disp_get_current_info(void) {
    return current_info;
}

int parse_current_line = 1;
int parse_current_col  = 1;

// 更新行列号的辅助函数
void disp_update_pos(int c) {
    if (c == '\n') {
        parse_current_line++;
        parse_current_col = 1;
    } else {
        parse_current_col++;
    }
}
