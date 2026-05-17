
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <ucontext.h>
#include <alloca.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

// 在 closure.c 或单独的头文件中定义
typedef struct eval_result {
    int kind;   // 0 = normal, 1 = tail_recurse
    union {
        disp_val *normal;
        struct {
            disp_val **new_args;
            int arg_count;
        } tail;
    };
} eval_result_t;

extern eval_result_t* result_nil();

extern eval_result_t* disp_eval_tail(disp_scope_t *env, disp_val *expr, int is_tail, disp_val *current_closure);
