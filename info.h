
#ifndef INFO_H
#define INFO_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef DEBUG
//#define DEBUG
#endif

// disp_info_t 定义
typedef struct disp_info {
    char *filename;
    int line;                 // 当前表达式位置（用于错误输出）
    int column;
    int saved_line;           // 解析器保存的行号
    int saved_col;            // 解析器保存的列号
    struct disp_info *next;
} disp_info_t;

// 线程局部信息管理（栈式）
void disp_init_info(void);                     // 初始化（确保 current_info 为 NULL）
disp_info_t* disp_get_current_info(void);      // 返回栈顶指针

#endif // INFO_H
