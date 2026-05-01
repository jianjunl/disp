
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

/* -------------------------------------------------------------------------
 * 帧类型不变，但不再内嵌 jmp_buf，改用 TRY/CATCH 实现跳转
 * ------------------------------------------------------------------------- */
typedef struct frame {
    int type;
    disp_val *tag;          // catch 的标签 / block 的名字 (符号)
    disp_val *result;       // 传递给 throw / return-from 的值
    disp_val *cleanup;      // unwind-protect 的清理表达式列表
    struct frame *prev;
} frame_t;

/* 所有帧都挂在这个线程局部链表上 */
_Thread_local frame_t *current_frame = NULL;

/* -------------------------------------------------------------------------
 * 用于从 throw / return-from 定位目标帧的线程局部变量
 * ------------------------------------------------------------------------- */
_Thread_local frame_t * volatile target_frame = NULL;   // 目标帧地址
_Thread_local int throw_code;                            // 配合 THROW 的返回值

/* -------------------------------------------------------------------------
 * 原始 run_cleanups 不再需要，清理由 unwind 帧的 CATCH 分支自动完成
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * throw 系统调用
 * ------------------------------------------------------------------------- */
__attribute__((optimize("O0")))
static disp_val* throw_syscall(disp_val **args, int count) {
    if (count < 1) ERET(NIL, "throw expects at least one argument");
    disp_val *tag = args[0];
    disp_val *value = (count >= 2) ? args[1] : NIL;

    /* 查找匹配的 catch 帧 */
    for (frame_t *f = current_frame; f; f = f->prev) {
        if (f->type == FRAME_CATCH && f->tag == tag) {
            f->result = value;
            target_frame = f;
            THROW(1);
            /* NOTREACHED */
        }
    }
    ERET(NIL, "throw: no matching catch frame for tag");
}

/* -------------------------------------------------------------------------
 * error 系统调用 (标签固定为 'error)
 * ------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------
 * catch 内置特殊形式
 * ------------------------------------------------------------------------- */
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

    TRY {
        disp_val *last = NIL;
        while (body && T(body) == DISP_CONS) {
            last = disp_eval(disp_car(body));
            body = disp_cdr(body);
        }
        current_frame = frame.prev;
        return last;
    }
    CATCH {
        /* 如果是针对本帧的 throw 异常，则由本帧处理 */
        if (THROWN == 1 && current_frame == &frame && &frame == target_frame) {
            disp_val *thrown = frame.result;
            current_frame = frame.prev;
            target_frame = NULL;   // 清理标志
            return thrown;
        }
        /* 否则传播异常：先弹出自身（如果是 unwind 帧则执行清理，这里不是） */
        current_frame = frame.prev;
        CAUGHT(THROWN);
    }
    END_TRY;
    return NIL;
}

/* -------------------------------------------------------------------------
 * block 内置特殊形式
 * ------------------------------------------------------------------------- */
__attribute__((optimize("O0")))
static disp_val* block_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "block: missing name");
    disp_val *name = disp_car(rest);
    /* 把符号 "nil" 转换为常量 NIL */
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

    TRY {
        disp_val *last = NIL;
        while (body && T(body) == DISP_CONS) {
            last = disp_eval(disp_car(body));
            body = disp_cdr(body);
        }
        current_frame = frame.prev;
        return last;
    }
    CATCH {
        /* 如果是针对本帧的 return-from / return 异常 */
        if (THROWN == 2 && current_frame == &frame && &frame == target_frame) {
            disp_val *returned = frame.result;
            current_frame = frame.prev;
            target_frame = NULL;   // 清理标志
            return returned;
        }
        /* 传播：弹出自身，如果是 unwind 帧会执行清理 */
        current_frame = frame.prev;
        CAUGHT(THROWN);
    }
    END_TRY;
    return NIL;
}

/* -------------------------------------------------------------------------
 * unwind-protect 内置特殊形式
 * ------------------------------------------------------------------------- */
static disp_val* unwind_protect_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "unwind-protect: missing protected form");
    disp_val *protected_form = disp_car(rest);
    disp_val *cleanup_forms = disp_cdr(rest);
    if (!cleanup_forms) ERET(NIL, "unwind-protect: missing cleanup forms");

    frame_t frame;
    frame.type = FRAME_UNWIND;
    frame.tag = NIL;
    frame.result = NIL;
    frame.cleanup = cleanup_forms;   /* 保存清理表达式列表 */
    frame.prev = current_frame;
    current_frame = &frame;

    TRY {
        disp_val *result = disp_eval(protected_form);
        /* 正常路径：先执行清理，再弹帧并返回 */
        disp_val *cl = cleanup_forms;
        while (cl && T(cl) == DISP_CONS) {
            disp_eval(disp_car(cl));
            cl = disp_cdr(cl);
        }
        current_frame = frame.prev;
        return result;
    }
    CATCH {
        /* 异常路径：执行清理，然后继续传播 */
        disp_val *cl = frame.cleanup;
        while (cl && T(cl) == DISP_CONS) {
            disp_eval(disp_car(cl));
            cl = disp_cdr(cl);
        }
        current_frame = frame.prev;
        CAUGHT(THROWN);
    }
    END_TRY;
    return NIL;
}

__attribute__((optimize("O0")))
static disp_val* return_from_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "return-from: missing name");
    disp_val *name = disp_car(rest);   // 不经求值
    if (T(name) != DISP_SYMBOL)
        ERET(NIL, "return-from: name must be a symbol");

    disp_val *value_rest = disp_cdr(rest);
    disp_val *value = (value_rest && T(value_rest) == DISP_CONS)
                      ? disp_eval(disp_car(value_rest))
                      : NIL;

    frame_t *target = NULL;
    for (frame_t *f = current_frame; f; f = f->prev) {
        if (f->type == FRAME_BLOCK && f->tag == name) {
            target = f;
            break;
        }
    }
    if (!target) ERET(NIL, "return-from: no matching block named");

    target->result = value;
    target_frame = target;
    THROW(2);
    return NIL;
}

__attribute__((optimize("O0")))
static disp_val* return_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    disp_val *value = (rest && T(rest) == DISP_CONS)
                      ? disp_eval(disp_car(rest))
                      : NIL;

    frame_t *target = NULL;
    for (frame_t *f = current_frame; f; f = f->prev) {
        if (f->type == FRAME_BLOCK && f->tag == NIL) {
            target = f;
            break;
        }
    }
    if (!target) ERET(NIL, "return: no matching nil block");

    target->result = value;
    target_frame = target;
    THROW(2);
    return NIL;
}

/* -------------------------------------------------------------------------
 * 模块初始化
 * ------------------------------------------------------------------------- */
void disp_init_module(void) {
    /* 将帧栈指针注册为 GC 根 */
    gc_add_root(&current_frame);
    /* 异常目标帧指针也需要作为根（否则其引用的帧可能被 GC 回收） */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    gc_add_root(&target_frame);
#pragma GCC diagnostic pop

    /* 注册内置函数，final = 1 */
    DEF("catch"         , MKB(catch_builtin         , "<#catch>"        ) , 1);
    DEF("throw"         , MKF(throw_syscall         , "<throw>"         ) , 1);
    DEF("error"         , MKF(error_syscall         , "<error>"         ) , 1);
    DEF("raise"         , MKF(error_syscall         , "<raise>"         ) , 1);
    DEF("block"         , MKB(block_builtin         , "<#block>"        ) , 1);
    DEF("return-from"   , MKB(return_from_builtin   , "<#return-from>"  ) , 1);  // 改 MKB
    DEF("return"        , MKB(return_builtin        , "<#return>"       ) , 1);  // 改 MKB
    DEF("unwind-protect", MKB(unwind_protect_builtin, "<#unwind-protect>"), 1);
}
