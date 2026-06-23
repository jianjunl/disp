
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

disp_val NIL, NaN, TRUE, QUIT, ELSE;
disp_val MODPATH, ARGS, IT;

disp_sid LAMBDA, MACRO, LET, LETA, LETREC, LETRECA, CONS, LIST;
disp_sid QUOTE, QUASIQUOTE, UNQUOTE, UNQUOTE_SPLICING, APPEND;
disp_sid IF, BEGIN, PROGN, COND, AND, OR;
disp_sid SET, SETQ, DEFINE, DEFUN;
disp_sid DEFAULT, RECV, SEND, AFTER, DO, DOTIMES, DOLIST;
disp_sid PROTO, THIS, DOT;
disp_val TYPE;

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
    disp_val fname = disp_make_str(info->filename);
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
        disp_val frame = disp_make_cons(disp_make_str(p->filename),
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
    if (tag == 0 || ((tag & 0xFFF8) == 0x7FF8) || TU(val) == TAG_LONG) {
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
    LAMBDA           = disp_get_sid("lambda");
    MACRO            = disp_get_sid("macro");
    LET              = disp_get_sid("let");
    LETA             = disp_get_sid("leta");
    LETREC           = disp_get_sid("letrec");
    LETRECA          = disp_get_sid("letreca");
    CONS             = disp_get_sid("cons");
    LIST             = disp_get_sid("list");
    QUOTE            = disp_get_sid("quote");
    QUASIQUOTE       = disp_get_sid("quasiquote");
    UNQUOTE          = disp_get_sid("unquote");
    UNQUOTE_SPLICING = disp_get_sid("unquote_splicing");
    APPEND           = disp_get_sid("append");
    IF               = disp_get_sid("if");
    BEGIN            = disp_get_sid("begin");
    PROGN            = disp_get_sid("progn");
    COND             = disp_get_sid("cond");
    AND              = disp_get_sid("and");
    OR               = disp_get_sid("or");
    SET              = disp_get_sid("set!");
    SETQ             = disp_get_sid("setq");
    DEFINE           = disp_get_sid("define");
    DEFUN            = disp_get_sid("defun");

    DEFAULT          = disp_get_sid("default");
    RECV             = disp_get_sid("recv");
    SEND             = disp_get_sid("send");
    AFTER            = disp_get_sid("after");
    DO               = disp_get_sid("do");
    DOTIMES          = disp_get_sid("dotimes");
    DOLIST           = disp_get_sid("dolist");
      
    char p[PATH_MAX] = "\0";
    if (!readlink("/proc/self/exe", p, sizeof(p) - 1)) {
	ERRO("readlink failed: %s\n", strerror (errno));
    } else { DBG("exec path is '%s'\n", p);
	*(strrchr(p, '/') + 1) = '\0';
        strcat(p, "../share/disp/modules/"); 
	DBG("disp module path is '%s'\n", p);
        MODPATH = MKS(p);
        disp_define_symbol_by_name(disp_global_env, ":path", MODPATH, 1);
    }

    REG("stdin"  , disp_make_file(stdin ,"r"), 1);
    REG("stdout" , disp_make_file(stdout,"w"), 1);
    REG("stderr" , disp_make_file(stderr,"w"), 1);

    REG("import", MKF(import_syscall , "<import>"), 1);
    REG("load"  , MKB(load_builtin   , "<#load>" ), 1);
    REG("repl"  , MKF(repl_syscall   , "<repl>"  ), 1);
    REG("gc"    , MKF(gc_syscall     , "<gc>"    ), 1);
    REG("info"  , MKF(info_syscall   , "<info>"  ), 1);
    REG("trace" , MKF(trace_syscall  , "<trace>" ), 1);

    // make else evaluate to true (so cond's default clause works)
    ELSE = REG("else", TRUE, 1);

    TYPE             = disp_make_symbol_by_name("type");
    DOT              = disp_get_sid(".");
    PROTO            = disp_get_sid("proto");
    THIS             = disp_get_sid("this");
    disp_val proto = disp_new_type(DNULL);
    REGI(PROTO, proto, 1);
    REGI(THIS , proto, 1);


    disp_import("disp.data.so");
    disp_import("disp.type.so");
    disp_import("disp.quote.so");
    disp_import("disp.lambda.so");
    disp_import("disp.let.so");
    disp_import("disp.letrec.so");

    disp_import("disp.define.so");
    disp_import("disp.cast.so");
    disp_import("disp.flow.so");
    disp_import("disp.set.so");
    disp_import("disp.do.so");
    disp_import("disp.dotimes.so");
    disp_import("disp.dolist.so");
    disp_import("disp.throw.so");
    disp_import("disp.apply.so");

    disp_import("disp.math.so");
    disp_import("disp.string.so");
    disp_import("disp.file.so");
    disp_import("disp.os.so");
    disp_import("disp.coro.so");
    disp_import("disp.thread.so");
}
