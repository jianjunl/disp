
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

disp_val *NIL, *TRUE, *QUIT;
disp_val *BYTE, *SHORT, *INT, *LONG, *FLOAT, *DOUBLE, *PNTR;
disp_val *LAMBDA, *LETA, *LETRECA;
disp_val *CONS, *LIST, *QUOTE, *QUASIQUOTE, *UNQUOTE, *UNQUOTE_SPLICING, *APPEND;
disp_val *MODPATH;


/* ======================== Built‑in 'load' ======================== */
static disp_val* load_syscall(disp_val **args, int argc) {
    if (argc != 1 || T(args[0]) != DISP_STRING) {
        ERET(NIL, "load expects a string filename\n");
    }
    DBG("load_syscall: about to load '%s'\n", disp_get_str(args[0]));
    return disp_load(disp_get_str(args[0]));
}


static disp_val* gc_syscall(disp_val **args, int count) {
    (void)args;
    if (count != 0)
        ERET(NIL, "gc expects no arguments");
    disp_gc();
    return TRUE;
}

// 内置函数：返回 (filename line column) 列表（栈顶）
static disp_val* info_syscall(disp_val **args, int arg_count) {
    (void)args; (void)arg_count;
    disp_info_t *info = disp_get_current_info();
    if (!info || !info->filename) return NIL;
    disp_val *fname = disp_make_string(info->filename);
    disp_val *line  = disp_make_int(info->line);
    disp_val *col   = disp_make_int(info->column);
    return disp_make_cons(fname, disp_make_cons(line, disp_make_cons(col, NIL)));
}

// 内置函数：返回调用栈列表，每个元素是 (filename line column)
static disp_val* trace_syscall(disp_val **args, int arg_count) {
    (void)args; (void)arg_count;
    disp_val *trace = NIL;
    for (disp_info_t *p = disp_get_current_info(); p; p = p->next) {
        if (!p->filename) continue;
        disp_val *frame = disp_make_cons(disp_make_string(p->filename),
                         disp_make_cons(disp_make_int(p->line),
                         disp_make_cons(disp_make_int(p->column), NIL)));
        trace = disp_make_cons(frame, trace);
    }
    return trace;  // 栈顶在列表头部
}

/* ======================== Initialisation ======================== */
void disp_init_globals() {

    char p[PATH_MAX] = "\0";
    if (!readlink("/proc/self/exe", p, sizeof(p) - 1)) {
	ERRO("readlink failed: %s\n", strerror (errno));
    } else { DBG("exec path is '%s'\n", p);
	*(strrchr(p, '/') + 1) = '\0';
        strcat(p, "../share/disp/modules/"); 
	DBG("disp module path is '%s'\n", p);
        MODPATH = MKS(p);
        DEF("...modpath", MODPATH, 1);
        gc_add_root(&MODPATH);
    }

    disp_init_gc();

    BYTE    = disp_define_type("byte"     , MKS(":byte"  ));
    SHORT   = disp_define_type("short"    , MKS(":short" ));
    INT     = disp_define_type("int"      , MKS(":int"   ));
    LONG    = disp_define_type("long"     , MKS(":long"  ));
    FLOAT   = disp_define_type("float"    , MKS(":float" ));
    DOUBLE  = disp_define_type("double"   , MKS(":double"));
    PNTR    = disp_define_type("pointer"  , MKS(":pntr"  ));
    gc_add_root(&BYTE);
    gc_add_root(&SHORT);
    gc_add_root(&INT);
    gc_add_root(&LONG);
    gc_add_root(&FLOAT);
    gc_add_root(&DOUBLE);
    gc_add_root(&PNTR);

    gc_add_root(&stdin);
    gc_add_root(&stdout);
    gc_add_root(&stderr);
    DEF("stdin"  , disp_make_file(stdin ,"r"), 1);
    DEF("stdout" , disp_make_file(stdout,"w"), 1);
    DEF("stderr" , disp_make_file(stderr,"w"), 1);

    DEF("load"  , MKF(load_syscall , "<load>" ), 1);
    gc_add_root(&load_syscall);
    DEF("gc"    , MKF(gc_syscall   , "<gc>"   ), 1);
    DEF("info"  , MKF(info_syscall , "<info>" ), 1);
    DEF("trace" , MKF(trace_syscall, "<trace>"), 1);
    gc_add_root(&info_syscall);
    gc_add_root(&trace_syscall);

    // make else evaluate to true (so cond's default clause works)
    DEF("else", TRUE, 1);

    disp_load("disp.data.so");
    CONS              = disp_find_symbol("cons");
    LIST              = disp_find_symbol("list");
    gc_add_root(&CONS);
    gc_add_root(&LIST);
    disp_load("disp.quote.so");
    APPEND            = disp_find_symbol("append");
    QUOTE             = disp_find_symbol("quote");
    QUASIQUOTE        = disp_find_symbol("quasiquote");
    UNQUOTE           = disp_find_symbol("unquote");
    UNQUOTE_SPLICING  = disp_find_symbol("unquote-splicing");
    gc_add_root(&APPEND);
    gc_add_root(&QUOTE);
    gc_add_root(&QUASIQUOTE);
    gc_add_root(&UNQUOTE);
    gc_add_root(&UNQUOTE_SPLICING);
    disp_load("disp.lamb.so");
    LAMBDA  = disp_find_symbol("lambda");
    disp_load("disp.leta.so");
    LETA    = disp_find_symbol("let*");
    LETRECA = disp_find_symbol("letrec*");
    disp_load("disp.flow.so");
    disp_load("disp.loop.so");
    disp_load("disp.throw.so");
    gc_add_root(&LAMBDA);
    gc_add_root(&LETA);
    gc_add_root(&LETRECA);
    disp_load("disp.math.so");
    disp_load("disp.string.so");
    disp_load("disp.file.so");
    disp_load("disp.os.so");
    disp_load("disp.coro.so");
    disp_load("disp.thread.so");
}

/* ======================== REPL ======================== */

void disp_repl() {

    printf("disp Lisp (type :quit to exit)\n");

    for (;;) {
        disp_info_t *_info = disp_get_current_info();
        if (_info && _info->filename) {
            printf("disp:");
            printf("%d", _info->line);
        } else printf("disp:");
        printf("> ");
        fflush(stdout);
/*
char *line = NULL;
size_t len = 0;
ssize_t nread = getline(&line, &len, stdin);
if (nread == -1) break;
fprintf(stderr, "RAW LINE: '%s' (len=%zu)\n", line, strlen(line));

FILE *mem = fmemopen(line, nread, "r");   // nread 包含换行符
if (!mem) {
    gc_free(line);
    break;
}

disp_val *expr = disp_read(mem);
fclose(mem);
gc_free(line);
*/
//sigset_t mask;
//sigemptyset(&mask);
//sigaddset(&mask, SIGUSR1);

//pthread_sigmask(SIG_BLOCK, &mask, NULL);
        disp_val *expr = disp_read(stdin);
//pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        if (!expr) break;
        disp_val *result = disp_eval(expr);
        disp_print(result);
        printf("\n");
        static int gc_counter = 0;
        if (++gc_counter > 1000) {
//            disp_gc();
            gc_counter = 0;
        }
    }
}
