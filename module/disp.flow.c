
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

// --- define ---
static disp_val* define_builtin(disp_val *expr) {
    disp_val *cadr = disp_cdr(expr);
    if (!cadr || T(cadr) != DISP_CONS) 
        ERET(NIL, "define: missing first argument");
    
    disp_val *first_arg = disp_car(cadr);
    
    if (T(first_arg) == DISP_SYMBOL) {
        // 可能是 (define symbol expr) 或 (define symbol (params) body ...)
        disp_val *rest = disp_cdr(cadr);
        
        // 检查是否为 (define symbol (params) body ...)
        if (rest && T(rest) == DISP_CONS) {
            disp_val *second = disp_car(rest);          // (params) 或 NIL
            disp_val *body_rest = disp_cdr(rest);       // 剩余表达式列表
            
            // 存在 body 且第二个参数是合法参数列表（NIL 或 CONS）
            if (body_rest && T(body_rest) == DISP_CONS) {
                // 构造 (lambda (params) body1 body2 ...)
                disp_val *params = second;              // 允许 NIL
                disp_val *lambda_expr = disp_make_cons(LAMBDA, 
                                        disp_make_cons(params, body_rest));
                disp_val *closure = disp_eval(lambda_expr);
                disp_define_symbol(disp_get_symbol_name(first_arg), closure, 0);
                return first_arg;
            }
        }
        
        // 普通 (define symbol expr)
        if (!rest || T(rest) != DISP_CONS) 
            ERET(NIL, "define: missing expression");
        disp_val *value = disp_eval(disp_car(rest));
        disp_define_symbol(disp_get_symbol_name(first_arg), value, 0);
        return first_arg;
        
    } else if (T(first_arg) == DISP_CONS) {
        // 原有 (define (name params) body ...) 语法
        disp_val *name_sym = disp_car(first_arg);
        if (T(name_sym) != DISP_SYMBOL) 
            ERET(NIL, "define: function name must be a symbol");
        disp_val *params = disp_cdr(first_arg);
        disp_val *rest = disp_cdr(cadr);
        if (!rest) ERET(NIL, "define: missing body");
        disp_val *lambda_expr = disp_make_cons(LAMBDA, disp_make_cons(params, rest));
        disp_val *closure = disp_eval(lambda_expr);
        disp_define_symbol(disp_get_symbol_name(name_sym), closure, 0);
        return name_sym;
        
    } else {
        ERET(NIL, "define: first argument must be symbol or list\n");
    }
}

// --- set! ---
static disp_val* setq_builtin(disp_val *expr) {
    // (set! symbol expr)
    disp_val *cadr = disp_cdr(expr);
    if (!cadr || T(cadr) != DISP_CONS) ERET(NIL, "set!: missing symbol");
    disp_val *sym = disp_car(cadr);
    if (T(sym) != DISP_SYMBOL) ERET(NIL, "set!: first argument must be a symbol");
    disp_val *rest = disp_cdr(cadr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "set!: missing expression");
    disp_val *value = disp_eval(disp_car(rest));
    disp_define_symbol(disp_get_symbol_name(sym), value, 0); // define updates the symbol's value
    return value;
}

// --- if ---
static disp_val* if_builtin(disp_val *expr) {
    // (if cond then [else])
    disp_val *cadr = disp_cdr(expr);
    if (!cadr || T(cadr) != DISP_CONS) ERET(NIL, "if: missing condition");
    disp_val *cond = disp_eval(disp_car(cadr));
    disp_val *then_rest = disp_cdr(cadr);
    if (!then_rest || T(then_rest) != DISP_CONS) ERET(NIL, "if: missing then clause");
    if (cond != NIL) {
        return disp_eval(disp_car(then_rest));
    } else {
        disp_val *else_rest = disp_cdr(then_rest);
        if (else_rest && T(else_rest) == DISP_CONS)
            return disp_eval(disp_car(else_rest));
        else
            return NIL;
    }
}

// --- cond ---
static disp_val* cond_builtin(disp_val *expr) {
    // (cond (test expr) ...)
    disp_val *clauses = disp_cdr(expr);
    while (clauses && T(clauses) == DISP_CONS) {
        disp_val *clause = disp_car(clauses);
        if (T(clause) != DISP_CONS) {
            ERET(NIL, "cond: malformed clause");
        }
        disp_val *test = disp_eval(disp_car(clause));
        if (test != NIL) {
            // evaluate the rest of the clause (one or more expressions)
            disp_val *body = disp_cdr(clause);
            if (!body || T(body) != DISP_CONS) return NIL;
            // evaluate all but return the last
            disp_val *last = NIL;
            while (body && T(body) == DISP_CONS) {
                last = disp_eval(disp_car(body));
                body = disp_cdr(body);
            }
            return last;
        }
        clauses = disp_cdr(clauses);
    }
    return NIL;
}

// --- while ---
static disp_val* while_builtin(disp_val *expr) {
    // (while test body...)
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) ERET(NIL, "while: missing test");
    disp_val *test_form = disp_car(rest);
    disp_val *body = disp_cdr(rest);
    if (!body) return NIL;
    disp_val *last = NIL;
    while (1) {
        disp_val *cond = disp_eval(test_form);
        if (cond == NIL) break;
        // evaluate body forms
        disp_val *body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            last = disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    return last;
}

// --- and ---
static disp_val* and_builtin(disp_val *expr) {
    disp_val *args = disp_cdr(expr);
    disp_val *last = TRUE;
    while (args && T(args) == DISP_CONS) {
        disp_val *val = disp_eval(disp_car(args));
        if (val == NIL) return NIL;
        last = val;
        args = disp_cdr(args);
    }
    return last;
}

// --- or ---
static disp_val* or_builtin(disp_val *expr) {
    disp_val *args = disp_cdr(expr);
    while (args && T(args) == DISP_CONS) {
        disp_val *val = disp_eval(disp_car(args));
        if (val != NIL) return val;
            args = disp_cdr(args);
        }
    return NIL;
}

// --- begin ---
static disp_val* begin_builtin(disp_val *expr) {
    // (begin expr1 expr2 ...)
    disp_val *body = disp_cdr(expr);
    if (!body) return NIL;           // 空 begin 返回 nil
    // 顺序求值所有表达式，返回最后一个的值
    disp_val *last = NIL;
    while (body && T(body) == DISP_CONS) {
        last = disp_eval(disp_car(body));
        body = disp_cdr(body);
    }
    return last;
}

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

// ======================== do 循环 ========================
// 语法: (do ((var init step) ...) (test result ...) body ...)
static disp_val* do_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "do: missing variable list");
    disp_val *var_specs = disp_car(rest);   // ((var init step) ...)
    rest = disp_cdr(rest);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "do: missing termination clause");
    disp_val *term_clause = disp_car(rest); // (test result ...)
    disp_val *body = disp_cdr(rest);        // body forms

    // 第一遍：收集变量名、初始值、步进表达式
    int var_count = 0;
    for (disp_val *s = var_specs; s && T(s) == DISP_CONS; s = disp_cdr(s))
        var_count++;
    if (var_count == 0) {
        // 没有变量，直接循环
        while (1) {
            // 求值终止条件
            disp_val *test = disp_eval(disp_car(term_clause));
            if (test != NIL) {
                // 返回结果表达式
                disp_val *results = disp_cdr(term_clause);
                if (!results) return NIL;
                disp_val *last = NIL;
                while (results && T(results) == DISP_CONS) {
                    last = disp_eval(disp_car(results));
                    results = disp_cdr(results);
                }
                return last;
            }
            // 执行 body
            disp_val *body_it = body;
            while (body_it && T(body_it) == DISP_CONS) {
                disp_eval(disp_car(body_it));
                body_it = disp_cdr(body_it);
            }
        }
    }

    // 分配存储
    char **var_names = gc_malloc(var_count * sizeof(char*));
    disp_val **init_vals = gc_malloc(var_count * sizeof(disp_val*));
    disp_val **step_exprs = gc_malloc(var_count * sizeof(disp_val*));
    int i = 0;
    for (disp_val *s = var_specs; s && T(s) == DISP_CONS; s = disp_cdr(s), i++) {
        disp_val *spec = disp_car(s);
        if (T(spec) != DISP_CONS) {
            gc_free(var_names); gc_free(init_vals); gc_free(step_exprs);
            ERET(NIL, "do: malformed variable spec");
        }
        disp_val *var = disp_car(spec);
        if (T(var) != DISP_SYMBOL) {
            gc_free(var_names); gc_free(init_vals); gc_free(step_exprs);
            ERET(NIL, "do: variable name must be a symbol");
        }
        var_names[i] = gc_strdup(disp_get_symbol_name(var));
        // 初始值表达式
        disp_val *init_part = disp_cdr(spec);
        if (!init_part || T(init_part) != DISP_CONS) {
            gc_free(var_names); gc_free(init_vals); gc_free(step_exprs);
            ERET(NIL, "do: missing init expression");
        }
        init_vals[i] = disp_car(init_part);
        // 步进表达式（可选）
        disp_val *step_part = disp_cdr(init_part);
        if (step_part && T(step_part) == DISP_CONS)
            step_exprs[i] = disp_car(step_part);
        else
            step_exprs[i] = NULL;  // 无步进
    }

    // 保存旧值并绑定初始值
    disp_val **old_vals = gc_malloc(var_count * sizeof(disp_val*));
    for (int j = 0; j < var_count; j++) {
        disp_val *old_sym = disp_find_symbol(var_names[j]);
        old_vals[j] = old_sym ? disp_get_symbol_value(old_sym) : NULL;
        disp_define_symbol(var_names[j], disp_eval(init_vals[j]), 0);
    }

    // 主循环
    disp_val *result = NIL;
    while (1) {
        // 检查终止条件
        disp_val *test = disp_eval(disp_car(term_clause));
        if (test != NIL) {
            // 求值结果表达式
            disp_val *results = disp_cdr(term_clause);
            if (!results) result = NIL;
            else {
                result = NIL;
                while (results && T(results) == DISP_CONS) {
                    result = disp_eval(disp_car(results));
                    results = disp_cdr(results);
                }
            }
            break;
        }
        // 执行 body
        disp_val *body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
        // 更新变量（并行更新需要保存新值，这里简化：顺序更新，但每个新值基于旧值）
        // 标准 do 是并行，但顺序更新对于简单用法足够，且更简单。
        disp_val **new_vals = gc_malloc(var_count * sizeof(disp_val*));
        for (int j = 0; j < var_count; j++) {
            if (step_exprs[j]) {
                new_vals[j] = disp_eval(step_exprs[j]);
            } else {
                // 无步进，保持当前值
                disp_val *sym = disp_find_symbol(var_names[j]);
                new_vals[j] = sym ? disp_get_symbol_value(sym) : NIL;
            }
        }
        for (int j = 0; j < var_count; j++) {
            disp_define_symbol(var_names[j], new_vals[j], 0);
        }
        gc_free(new_vals);
    }

    // 恢复旧值
    for (int j = 0; j < var_count; j++) {
        if (old_vals[j] != NULL)
            disp_define_symbol(var_names[j], old_vals[j], 0);
        else
            disp_define_symbol(var_names[j], NIL, 0);
        gc_free(var_names[j]);
    }
    gc_free(var_names);
    gc_free(init_vals);
    gc_free(step_exprs);
    gc_free(old_vals);
    return result;
}

// ======================== dotimes 循环 ========================
// 语法: (dotimes (var count [result]) body ...)
static disp_val* dotimes_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "dotimes: missing binding list");
    disp_val *binding = disp_car(rest);
    if (T(binding) != DISP_CONS)
        ERET(NIL, "dotimes: malformed binding");
    disp_val *var = disp_car(binding);
    if (T(var) != DISP_SYMBOL)
        ERET(NIL, "dotimes: variable must be a symbol");
    const char *var_name = disp_get_symbol_name(var);
    disp_val *count_expr = disp_car(disp_cdr(binding));
    if (!count_expr)
        ERET(NIL, "dotimes: missing count");
    disp_val *result_expr = NIL;
    disp_val *rest_binding = disp_cdr(disp_cdr(binding));
    if (rest_binding && T(rest_binding) == DISP_CONS)
        result_expr = disp_car(rest_binding);
    disp_val *body = disp_cdr(rest);  // 剩余 body 形式

    // 求值 count
    disp_val *count_val = disp_eval(count_expr);
    long limit;
    disp_flag_t ct = T(count_val);
    if (ct == DISP_INT) {
        limit = disp_get_int(count_val);
    } else if (ct == DISP_LONG) {
        limit = disp_get_long(count_val);
    } else {
        ERET(NIL, "dotimes: count must be an integer");
    }
    if (limit <= 0) {
        // 直接返回 result 或 nil
        if (result_expr)
            return disp_eval(result_expr);
        else
            return NIL;
    }

    // 保存旧值
    disp_val *old_sym = disp_find_symbol(var_name);
    disp_val *old_val = old_sym ? disp_get_symbol_value(old_sym) : NULL;
    // 循环
    disp_val *last_result = NIL;
    for (long i = 0; i < limit; i++) {
        disp_define_symbol(var_name, disp_make_long(i), 0);
        // 执行 body
        disp_val *body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            last_result = disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    // 恢复旧值
    if (old_sym)
        disp_define_symbol(var_name, old_val, 0);
    else
        disp_define_symbol(var_name, NIL, 0);
    // 返回结果
    if (result_expr)
        return disp_eval(result_expr);
    else
        return last_result ? last_result : NIL;
}

// ======================== dolist 循环 ========================
// 语法: (dolist (var list [result]) body ...)
static disp_val* dolist_builtin(disp_val *expr) {
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS)
        ERET(NIL, "dolist: missing binding list");
    disp_val *binding = disp_car(rest);
    if (T(binding) != DISP_CONS)
        ERET(NIL, "dolist: malformed binding");
    disp_val *var = disp_car(binding);
    if (T(var) != DISP_SYMBOL)
        ERET(NIL, "dolist: variable must be a symbol");
    const char *var_name = disp_get_symbol_name(var);
    disp_val *list_expr = disp_car(disp_cdr(binding));
    if (!list_expr)
        ERET(NIL, "dolist: missing list");
    disp_val *result_expr = NIL;
    disp_val *rest_binding = disp_cdr(disp_cdr(binding));
    if (rest_binding && T(rest_binding) == DISP_CONS)
        result_expr = disp_car(rest_binding);
    disp_val *body = disp_cdr(rest);  // 剩余 body

    // 求值列表
    disp_val *lst = disp_eval(list_expr);
    // 保存旧值
    disp_val *old_sym = disp_find_symbol(var_name);
    disp_val *old_val = old_sym ? disp_get_symbol_value(old_sym) : NULL;
    // 遍历
    disp_val *last_result = NIL;
    for (disp_val *p = lst; p && T(p) == DISP_CONS; p = disp_cdr(p)) {
        disp_val *elem = disp_car(p);
        disp_define_symbol(var_name, elem, 0);
        // 执行 body
        disp_val *body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            last_result = disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    // 恢复旧值
    if (old_sym)
        disp_define_symbol(var_name, old_val, 0);
    else
        disp_define_symbol(var_name, NIL, 0);
    // 返回结果
    if (result_expr)
        return disp_eval(result_expr);
    else
        return last_result ? last_result : NIL;
}

static disp_val* repeat_builtin(disp_val *expr) {
    // 语法: (repeat count body ...)
    disp_val *rest = disp_cdr(expr);
    if (!rest || T(rest) != DISP_CONS) 
        ERET(NIL, "repeat: missing count");
    disp_val *count_val = disp_eval(disp_car(rest));
    long n = (T(count_val) == DISP_INT) ? disp_get_int(count_val) 
            : (T(count_val) == DISP_LONG) ? disp_get_long(count_val) 
            : 0;
    if (n <= 0) return NIL;
    disp_val *body = disp_cdr(rest);
    if (!body) return NIL;
    disp_val *last = NIL;
    for (long i = 0; i < n; i++) {
        disp_val *body_it = body;
        while (body_it && T(body_it) == DISP_CONS) {
            last = disp_eval(disp_car(body_it));
            body_it = disp_cdr(body_it);
        }
    }
    return last ? last : NIL;
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    gc_add_root(&current_frame);
    DEF("define"  , MKB(define_builtin , "<#define>" ), 1);
    DEF("set!"    , MKB(setq_builtin   , "<#set!>"   ), 1);
    DEF("if"      , MKB(if_builtin     , "<#if>"     ), 1);
    DEF("cond"    , MKB(cond_builtin   , "<#cond>"   ), 1);
    DEF("while"   , MKB(while_builtin  , "<#while>"  ), 1);
    DEF("and"     , MKB(and_builtin    , "<#and>"    ), 1);
    DEF("or"      , MKB(or_builtin     , "<#or>"     ), 1);
    DEF("begin"   , MKB(begin_builtin  , "<#begin>"  ), 1);
    DEF("progn"   , MKB(begin_builtin  , "<#progn>"  ), 1);
    DEF("catch"         , MKB(catch_builtin         , "<#catch>"        ) , 1);
    DEF("throw"         , MKF(throw_syscall         , "<throw>"         ) , 1);
    DEF("error"         , MKF(error_syscall         , "<error>"         ) , 1);
    DEF("raise"         , MKF(error_syscall         , "<raise>"         ) , 1);
    DEF("block"         , MKB(block_builtin         , "<#block>"        ) , 1);
    DEF("return-from"   , MKF(return_from_syscall   , "<return-from>"   ) , 1);
    DEF("return"        , MKF(return_syscall        , "<return>"        ) , 1);
    DEF("unwind-protect", MKB(unwind_protect_builtin, "<#unwind-protect>"), 1);
    DEF("do"      , MKB(do_builtin     , "<#do>"     ), 1);
    DEF("dotimes" , MKB(dotimes_builtin, "<#dotimes>"), 1);
    DEF("dolist"  , MKB(dolist_builtin , "<#dolist>" ), 1);
    DEF("repeat"  , MKB(repeat_builtin , "<#repeat>" ), 1);
}
