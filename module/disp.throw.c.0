
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

// 帧类型
#define FRAME_CATCH  0
#define FRAME_BLOCK  1
#define FRAME_UNWIND 2   // unwind-protect 的清理帧

typedef struct frame {
    int type;
    gc_jmp_buf_t env;
    disp_val *tag;          // catch 的 tag 或 block 的名字（符号）
    disp_val *result;       // 传递给 throw/return-from 的值
    disp_val *cleanup;      // unwind-protect 的清理表达式列表
    struct frame *prev;
} frame_t;

_Thread_local frame_t *current_frame = NULL;

static void run_cleanups(frame_t *from, frame_t *to) {
    for (frame_t *f = from; f != to; f = f->prev) {
        if (f->type == FRAME_UNWIND && f->cleanup) {
            disp_val *cleanup = f->cleanup;
            while (cleanup && T(cleanup) == DISP_CONS) {
                disp_eval(disp_car(cleanup));
                cleanup = disp_cdr(cleanup);
            }
        }
    }
}

__attribute__((optimize("O0")))
static disp_val* throw_syscall(disp_val **args, int count) {
    if (count < 1) ERET(NIL, "throw expects at least one argument");
    disp_val *tag = args[0];
    disp_val *value = (count >= 2) ? args[1] : NIL;
    
    frame_t *target = NULL;
    for (frame_t *f = current_frame; f; f = f->prev) {
        if (f->type == FRAME_CATCH && f->tag == tag) {
            target = f;
            break;
        }
    }
    if (!target) ERET(NIL, "throw: no matching catch frame for tag");
    
    // 执行从 current_frame 到 target->prev 之间的所有清理帧
    run_cleanups(current_frame, target->prev);
    
    target->result = value;
    gc_longjmp(&target->env, 1);
    return NIL;
}

/* (error message ...) -> 抛出 'error 标签 */
static disp_val* error_syscall(disp_val **args, int count) {
    if (count < 1) ERET(NIL, "error: expects at least one argument");
    
    disp_val *throw_args[2];
    throw_args[0] = disp_intern_symbol("error");
    
    if (count == 1) {
        throw_args[1] = args[0];
    } else {
        /* 多个参数组成列表作为错误数据 */
        disp_val *lst = NIL;
        for (int i = count - 1; i >= 0; i--) {
            lst = disp_make_cons(args[i], lst);
        }
        throw_args[1] = lst;
    }
    
    return throw_syscall(throw_args, 2);
}

__attribute__((optimize("O0")))
static disp_val* return_from_syscall(disp_val **args, int count) {
    if (count < 1) ERET(NIL, "return-from expects at least one argument");
    disp_val *name = args[0];
    if (T(name) != DISP_SYMBOL) ERET(NIL, "return-from: name must be a symbol");
    disp_val *value = (count >= 2) ? args[1] : NIL;
    
    frame_t *target = NULL;
    for (frame_t *f = current_frame; f; f = f->prev) {
        if (f->type == FRAME_BLOCK && f->tag == name) {
            target = f;
            break;
        }
    }
    if (!target) ERET(NIL, "return-from: no matching block named");
    
    // 执行从 current_frame 到 target->prev 之间的所有清理帧
    run_cleanups(current_frame, target->prev);
    
    target->result = value;
    gc_longjmp(&target->env, 1);
    return NIL;
}

// --- catch ---
__attribute__((optimize("O0")))
static disp_val* catch_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "catch: missing tag");
    disp_val *tag = disp_eval(disp_car(rest));
    disp_val *body = disp_cdr(rest);
    if (!body) ERET(NIL, "catch: missing body");
    
    frame_t frame;
    frame.type = FRAME_CATCH;
    frame.tag = tag;
    frame.result = NIL;
    frame.cleanup = NIL;
    frame.prev = current_frame;
    current_frame = &frame;
    
    if (gc_setjmp(&frame.env) == 0) {
        disp_val *last = NIL;
        while (body && T(body) == DISP_CONS) {
            last = disp_eval(disp_car(body));
            body = disp_cdr(body);
        }
        current_frame = frame.prev;
        return last;
    } else {
        disp_val *thrown = frame.result;
        current_frame = frame.prev;
        return thrown;
    }
}

__attribute__((optimize("O0")))
static disp_val* block_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "block: missing name");
    disp_val *name = disp_car(rest);
    // Convert the symbol "nil" to the NIL constant
    if (T(name) == DISP_SYMBOL && strcmp(disp_get_symbol_name(name), "nil") == 0) {
        name = NIL;
    }
    if (T(name) != DISP_SYMBOL && name != NIL) 
        ERET(NIL, "block: name must be a symbol or nil");
    disp_val *body = disp_cdr(rest);
    if (!body) ERET(NIL, "block: missing body");
    
    frame_t frame;
    frame.type = FRAME_BLOCK;
    frame.tag = name;
    frame.result = NIL;
    frame.cleanup = NIL;
    frame.prev = current_frame;
    current_frame = &frame;
    
    if (gc_setjmp(&frame.env) == 0) {
        disp_val *last = NIL;
        while (body && T(body) == DISP_CONS) {
            last = disp_eval(disp_car(body));
            body = disp_cdr(body);
        }
        current_frame = frame.prev;
        return last;
    } else {
        disp_val *returned = frame.result;
        current_frame = frame.prev;
        return returned;
    }
}

__attribute__((optimize("O0")))
static disp_val* return_syscall(disp_val **args, int count) {
    // (return [value]) -> 从最近的 nil 块返回
    disp_val *value = (count >= 1) ? args[0] : NIL;
    
    frame_t *target = NULL;
    for (frame_t *f = current_frame; f; f = f->prev) {
        if (f->type == FRAME_BLOCK && f->tag == NIL) {
            target = f;
            break;
        }
    }
    if (!target) ERET(NIL, "return: no matching nil block");
    
    // 执行清理帧
    run_cleanups(current_frame, target->prev);
    
    target->result = value;
    gc_longjmp(&target->env, 1);
    return NIL;
}

static disp_val* unwind_protect_builtin(disp_val *expr) {
    // 语法: (unwind-protect protected-form cleanup-form ...)
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "unwind-protect: missing protected form");
    disp_val *protected_form = disp_car(rest);
    disp_val *cleanup_forms = disp_cdr(rest);
    if (!cleanup_forms) ERET(NIL, "unwind-protect: missing cleanup forms");
    
    // 创建一个清理帧，但不设置 setjmp。清理帧的作用仅仅是在非本地退出时被 run_cleanups 执行。
    // 我们将清理表达式列表存储在一个特殊帧中，并将它推入帧栈。
    frame_t frame;
    frame.type = FRAME_UNWIND;
    frame.tag = NIL;
    frame.result = NIL;
    frame.cleanup = cleanup_forms;   // 保存清理表达式列表
    frame.prev = current_frame;
    current_frame = &frame;
    
    // 执行受保护形式
    disp_val *result = disp_eval(protected_form);
    
    // 正常退出：先执行清理，然后返回结果
    // 注意：正常退出时也要执行清理（与 Common Lisp 一致）
    while (cleanup_forms && T(cleanup_forms) == DISP_CONS) {
        disp_eval(disp_car(cleanup_forms));
        cleanup_forms = disp_cdr(cleanup_forms);
    }
    // 弹出清理帧
    current_frame = frame.prev;
    return result;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    gc_add_root(&current_frame);
    DEF("catch"         , MKB(catch_builtin         , "<#catch>"        ) , 1);
    DEF("throw"         , MKF(throw_syscall         , "<throw>"         ) , 1);
    DEF("error"         , MKF(error_syscall         , "<error>"         ) , 1);
    DEF("raise"         , MKF(error_syscall         , "<raise>"         ) , 1);
    DEF("block"         , MKB(block_builtin         , "<#block>"        ) , 1);
    DEF("return-from"   , MKF(return_from_syscall   , "<return-from>"   ) , 1);
    DEF("return"        , MKF(return_syscall        , "<return>"        ) , 1);
    DEF("unwind-protect", MKB(unwind_protect_builtin, "<#unwind-protect>"), 1);
}
