
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
        disp_val normal;
        struct {
            disp_val *new_args;
            int arg_count;
            disp_val target;      // 要调用的目标函数
        } tail;
    };
} eval_result_t;

#define RESULT_NORMAL(v) (eval_result_t){.kind = 0, .normal = v}
#define RESULT_TAIL(t, v, c) (eval_result_t) { \
    .kind = 1, \
    .tail.new_args = v, \
    .tail.arg_count = c, \
    .tail.target = t \
}

eval_result_t disp_eval_tail(disp_env_t *env, disp_val expr, int is_tail, disp_val current_closure);
eval_result_t disp_eval_tail_flow(disp_env_t *env, disp_val expr, int is_tail, disp_val current_closure);
eval_result_t disp_eval_tail_let(disp_env_t *env, disp_val expr, int is_tail, disp_val current_closure);
eval_result_t disp_eval_tail_leta(disp_env_t *env, disp_val expr, int is_tail, disp_val current_closure);
eval_result_t disp_eval_tail_letrec(disp_env_t *env, disp_val expr, int is_tail, disp_val current_closure);
eval_result_t disp_eval_tail_letreca(disp_env_t *env, disp_val expr, int is_tail, disp_val current_closure);
