
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

//disp_val __attribute__((section("gc_roots"))) disp_builtin_roots[NUM_BUILTIN_ROOTS] = {};
disp_val disp_builtin_roots[NUM_BUILTIN_ROOTS] = {};

/* ======================== Built‑in 'load' ======================== */
static disp_val import_syscall(disp_val *args, int argc) {
    if (argc != 1 || T(args[0]) != FLAG_STRING) {
        ERET(NIL, "import expects a string filename\n");
    }
    DBG("import_syscall: about to load '%s'\n", disp_get_str(args[0]));
    return disp_import(disp_get_str(args[0]));
}

static disp_val load_builtin(disp_env_t *env, disp_val expr) {
   disp_val rest = disp_cdr(expr);
    if (E(rest, NIL)) {
        ERET(NIL, "load expects a string filename\n");
    }
    if (NE(disp_cdr(rest), NIL)) {
        ERRO("load: warning - extra arguments ignored");
    }
    disp_val arg0 = disp_eval(env, disp_car(rest));
    if (T(arg0) != FLAG_STRING) {
        ERET(NIL, "load expects a string filename\n");
    }
    DBG("load_builtin: about to load '%s'\n", disp_get_str(args0));
    return disp_load(env, disp_get_str(arg0));
}

static disp_val repl_syscall(disp_val *args, int count) {
    (void)args;
    if (count != 0)
        ERET(NIL, "repl expects no arguments");
    disp_repl();
    return TRUE;
}

static disp_val gc_syscall(disp_val *args, int count) {
    (void)args;
    if (count != 0)
        ERET(NIL, "gc expects no arguments");
    gc_collect();
    gc_dump_stats();
    return TRUE;
}

// 内置函数：返回 (filename line column) 列表（栈顶）
static disp_val info_syscall(disp_val *args, int arg_count) {
    (void)args; (void)arg_count;
    disp_info_t *info = disp_get_current_info();
    if (!info || !info->filename) return NIL;
    disp_val fname = disp_make_string(info->filename);
    disp_val line  = disp_make_int(info->line);
    disp_val col   = disp_make_int(info->column);
    return disp_make_cons(fname, disp_make_cons(line, disp_make_cons(col, NIL)));
}

// 内置函数：返回调用栈列表，每个元素是 (filename line column)
static disp_val trace_syscall(disp_val *args, int arg_count) {
    (void)args; (void)arg_count;
    disp_val trace = NIL;
    for (disp_info_t *p = disp_get_current_info(); p; p = p->next) {
        if (!p->filename) continue;
        disp_val frame = disp_make_cons(disp_make_string(p->filename),
                         disp_make_cons(disp_make_int(p->line),
                         disp_make_cons(disp_make_int(p->column), NIL)));
        trace = disp_make_cons(frame, trace);
    }
    return trace;  // 栈顶在列表头部
}

#if DISP_NAN_BOXING
static void* disp_gc_validate(void *ptr) {
    uint64_t val = (uint64_t)ptr;
    uint64_t tag = val >> 48;

    // 情形1: 高16位为0 => 真实指针（来自精确根或已解码地址），直接返回
    // 情形2: 高16位在 0x7FF8 ~ 0x7FFF 之间 => 装箱指针，返回低48位
    // 情形3: 其他 => 数值或无效，返回 NULL
    //if ((tag & 0xF) < 10) {
    //if (tag == 0 || ((tag & 0xFFF8) == 0x7FF8) || tag == FLAG_LONG || tag == FLAG_DOUBLE) {
    if (tag == 0 || ((tag & 0xFFF8) == 0x7FF8) || T(val) == TAG_LONG) {
        // 注意：情形2 需保证指针 tag 区域为 0x7FF8~0x7FFF，掩码 0xFFF8 恰好筛选出这些值
        return (void*)(val & 0x0000FFFFFFFFFFFFULL);
    }
    return NULL;
}
#elif DISP_BOXING
static void* disp_gc_validate(void *ptr) {
    uint64_t val = (uint64_t)ptr;
    uint8_t tag = (uint8_t)(val >> 56);

    if (tag == 0 || !(tag & 0x80)) {
        return (void*)(val & 0x00FFFFFFFFFFFFFFULL);
    }
    return NULL;
}
#endif

/* ======================== Initialisation ======================== */
void disp_init() {
    gc_init();
#if DISP_BOXING
    gc_set_validate_hook(disp_gc_validate);
#endif
    disp_init_info();
    disp_init_name_table();
    disp_init_env();
    disp_init_symbol();
      
    char p[PATH_MAX] = "\0";
    if (!readlink("/proc/self/exe", p, sizeof(p) - 1)) {
	ERRO("readlink failed: %s\n", strerror (errno));
    } else { DBG("exec path is '%s'\n", p);
	*(strrchr(p, '/') + 1) = '\0';
        strcat(p, "../share/disp/modules/"); 
	DBG("disp module path is '%s'\n", p);
        MODPATH = MKS(p);
        DEF(":path", MODPATH, 1);
    }

    BYTE    = disp_define_type("byte"     , MKS(":byte"  ));
    SHORT   = disp_define_type("short"    , MKS(":short" ));
    INT     = disp_define_type("int"      , MKS(":int"   ));
    LONG    = disp_define_type("long"     , MKS(":long"  ));
    FLOAT   = disp_define_type("float"    , MKS(":float" ));
    DOUBLE  = disp_define_type("double"   , MKS(":double"));
    PNTR    = disp_define_type("pointer"  , MKS(":pntr"  ));

    DEF("stdin"  , disp_make_file(stdin ,"r"), 1);
    DEF("stdout" , disp_make_file(stdout,"w"), 1);
    DEF("stderr" , disp_make_file(stderr,"w"), 1);

    DEF("import", MKF(import_syscall , "<import>"), 1);
    DEF("load"  , MKB(load_builtin   , "<#load>" ), 1);
    DEF("repl"  , MKF(repl_syscall   , "<repl>"  ), 1);
    DEF("gc"    , MKF(gc_syscall     , "<gc>"    ), 1);
    DEF("info"  , MKF(info_syscall   , "<info>"  ), 1);
    DEF("trace" , MKF(trace_syscall  , "<trace>" ), 1);

    // make else evaluate to true (so cond's default clause works)
    DEF("else"   , TRUE, 1); ELSE    = SYMBOL_BY_NAME(disp_global_env, "else");
    DEF("default", NIL , 0); DEFAULT = SYMBOL_BY_NAME(disp_global_env, "default");
    DEF("it"     , NIL , 0); IT      = SYMBOL_BY_NAME(disp_global_env, "it");
    DEF("recv"   , NIL , 0); RECV    = SYMBOL_BY_NAME(disp_global_env, "recv");
    DEF("send"   , NIL , 0); SEND    = SYMBOL_BY_NAME(disp_global_env, "send");
    DEF("after"  , NIL , 0); AFTER   = SYMBOL_BY_NAME(disp_global_env, "after");
///*
///*
    disp_import("disp.data.so");
    CONS              = SYMBOL_BY_NAME(disp_global_env, "cons");
    LIST              = SYMBOL_BY_NAME(disp_global_env, "list");
    disp_import("disp.quote.so");
    APPEND            = SYMBOL_BY_NAME(disp_global_env, "append");
    QUOTE             = SYMBOL_BY_NAME(disp_global_env, "quote");
    QUASIQUOTE        = SYMBOL_BY_NAME(disp_global_env, "quasiquote");
    UNQUOTE           = SYMBOL_BY_NAME(disp_global_env, "unquote");
    UNQUOTE_SPLICING  = SYMBOL_BY_NAME(disp_global_env, "unquote-splicing");
    disp_import("disp.lambda.so");
    LAMBDA  = SYMBOL_BY_NAME(disp_global_env, "lambda");
    disp_import("disp.let.so");
    LET     = SYMBOL_BY_NAME(disp_global_env, "let");
    LETA    = SYMBOL_BY_NAME(disp_global_env, "let*");
    disp_import("disp.letrec.so");
    LETREC  = SYMBOL_BY_NAME(disp_global_env, "letrec");
    LETRECA = SYMBOL_BY_NAME(disp_global_env, "letrec*");

    disp_import("disp.define.so");
    disp_import("disp.flow.so");
    disp_import("disp.do.so");
    disp_import("disp.dotimes.so");
    disp_import("disp.dolist.so");
    disp_import("disp.throw.so");

    disp_import("disp.math.so");
    disp_import("disp.string.so");
    disp_import("disp.file.so");
    disp_import("disp.os.so");
    disp_import("disp.coro.so");
    disp_import("disp.thread.so");
    IF     = SYMBOL_BY_NAME(disp_global_env, "if");
    BEGIN  = SYMBOL_BY_NAME(disp_global_env, "begin");
    PROGN  = SYMBOL_BY_NAME(disp_global_env, "progn");
    COND   = SYMBOL_BY_NAME(disp_global_env, "cond");
    AND    = SYMBOL_BY_NAME(disp_global_env, "and");
    OR     = SYMBOL_BY_NAME(disp_global_env, "or");
    SET    = SYMBOL_BY_NAME(disp_global_env, "set!");
    SETQ   = SYMBOL_BY_NAME(disp_global_env, "setq");
    DEFINE = SYMBOL_BY_NAME(disp_global_env, "define");
//*/
}
