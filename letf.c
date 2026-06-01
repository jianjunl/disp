
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

disp_val disp_letf(disp_env_t *env, disp_val expr) {
    // 解析命名 let 语法
    disp_val rest = disp_cdr(expr);
    if (N(rest) || T(rest) != FLAG_CONS) ERET(NIL, "named let: missing name");
    disp_val name = disp_car(rest);
    if (T(name) != FLAG_SYMBOL) ERET(NIL, "named let: name must be a symbol");
    
    disp_val bindings_rest = disp_cdr(rest);
    if (N(bindings_rest) || T(bindings_rest) != FLAG_CONS) ERET(NIL, "named let: missing bindings");
    disp_val bindings = disp_car(bindings_rest);
    disp_val body = disp_cdr(bindings_rest);
    if (N(body)) ERET(NIL, "named let: missing body");
    
    // 提取变量和初值
    int var_count = 0;
    for (disp_val b = bindings; NN(b) && T(b) == FLAG_CONS; b = disp_cdr(b)) var_count++;
    GC_ROOT(disp_val, var_syms) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    GC_ROOT(disp_val, init_exprs) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    int idx = 0;
    for (disp_val b = bindings; NN(b) && T(b) == FLAG_CONS; b = disp_cdr(b)) {
        disp_val pair = disp_car(b);
        if (T(pair) != FLAG_CONS || T(disp_car(pair)) != FLAG_SYMBOL) {
            gc_free(init_exprs); gc_free(var_syms);
            ERET(NIL, "named let: malformed binding");
        }
        var_syms[idx] = disp_car(pair);
        init_exprs[idx] = disp_car(disp_cdr(pair));
        idx++;
        b = disp_cdr(b);
    }

    // 创建循环作用域
    GC_ROOT(disp_env_t, loop_env) = disp_new_env(env);
    
    // 1. 先绑定所有变量初值（此时闭包尚未创建）
    for (int i = 0; i < var_count; i++) {
        disp_val init_val = disp_eval(env, init_exprs[i]);
        uint64_t vid = SI(var_syms[i]);
        disp_define_symbol(loop_env, vid, init_val, 0);
    }
    
    // 2. 提取当前变量的值作为调用闭包的初始参数（此时所有变量都是初值）
    GC_ROOT(disp_val, args) = gc_typed_malloc(var_count * sizeof(disp_val), &GC_TYPE_PTR_ARRAY);
    for (int i = 0; i < var_count; i++) {
        uint64_t vid = SI(var_syms[i]);
        disp_val sym = disp_find_symbol(loop_env, vid);
        args[i] = SV(sym);
    }
    
    // 3. 创建闭包（参数列表为变量符号，body 为原 body）
    disp_val params = NIL;
    for (int i = var_count - 1; i >= 0; i--) 
        params = disp_make_cons(var_syms[i], params);
    disp_val closure = disp_make_closure(loop_env, params, body, 1);
    
    // 4. 将闭包绑定到 loop_env 中的同名符号
    disp_define_symbol(loop_env, SI(name), closure, 1);
    
    // 5. 调用闭包（传入初始参数，这些参数不会覆盖闭包自身的绑定）
    disp_val result = disp_apply_closure(closure, args, var_count);
    
    // 清理
    gc_free(args);
    gc_free(var_syms); gc_free(init_exprs);
    
    return result;
}
