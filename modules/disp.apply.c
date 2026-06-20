
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

/* ----- apply ----- */
static disp_val apply_syscall(disp_val *args, int count) {
    if (count != 2) {
        ERET(NIL, "apply: expects two arguments (func arg-list)");
    }
    disp_val func = args[0];
    disp_val arg_list = args[1];

    // 检查参数列表是否为 proper list（末尾必须是 nil）
    disp_val p = arg_list;
    while (NE(p, NIL) && T(p) == FLAG_CONS) {
        p = disp_cdr(p);
    }
    if (NE(p, NIL)) {
        ERET(NIL, "apply: argument list must be a proper list");
    }

    // 计算列表长度
    int arg_count = 0;
    for (disp_val a = arg_list; NE(a, NIL); a = disp_cdr(a)) {
        arg_count++;
    }

    // 分配参数数组
    disp_val *argv = gc_typed_malloc(arg_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    if (!argv) return NIL;
    int i = 0;
    for (disp_val a = arg_list; NE(a, NIL); a = disp_cdr(a)) {
        argv[i++] = disp_car(a);
    }

    disp_val result = NIL;
    if (T(func) == FLAG_SYSCALL) {
        result = disp_get_syscall(func)(argv, arg_count);
    } else if (T(func) == FLAG_CLOSURE) {
        result = disp_apply_closure(func, argv, arg_count);
    } else if (T(func) == FLAG_BUILTIN) {
        // 内置特殊形式通常不能通过 apply 直接调用，但我们可以尝试将列表构造成表达式再求值？
        // 简单处理：报错
        gc_free(argv);
        ERET(NIL, "apply: cannot apply built-in special form");
    } else {
        gc_free(argv);
        ERET(NIL, "apply: first argument must be a function or closure");
    }

    gc_free(argv);
    return result;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    REG("apply"   , MKF(apply_syscall  , "<apply>"   ), 1);
}
