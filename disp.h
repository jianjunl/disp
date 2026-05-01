
#ifndef DISP_H
#define DISP_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef DEBUG
//#define DEBUG
#endif
#include "gc/gc.h"
#include "disp.macro.h"

/* Value types */
typedef enum {
    DISP_VOID,
    DISP_BYTE, DISP_SHORT, DISP_INT, DISP_LONG, DISP_FLOAT, DISP_DOUBLE, DISP_STRING, DISP_CONS,
    DISP_SYMBOL,
    DISP_SYSCALL, DISP_BUILTIN, DISP_CLOSURE, DISP_MACRO,
    DISP_FILE,
    DISP_CORO, DISP_CHAN, DISP_SOCKET,
    DISP_THREAD, DISP_MUTEX, DISP_COND,
    DISP_TYPE
} disp_flag_t;

/* Opaque types */
typedef union disp_data disp_data;

typedef struct disp_val {
    disp_flag_t flag;
    disp_data *data;
} disp_val;

/* Function pointer type for built‑in functions */
typedef disp_val* (*disp_syscall_t)(disp_val** args, int arg_count);
typedef disp_val* (*disp_builtin_t)(disp_val* arg);

/* all-in-one union for testing */
#define DISP_H_DISP_ALLINONE_UNION_H
//#include "disp_allinone_union.h"
#undef DISP_H_DISP_ALLINONE_UNION_H

/* --- Value constructors --- */
void disp_lock_table(void);
void disp_unlock_table(void);
char* disp_get_symbol_name(disp_val *v);
disp_val* disp_get_symbol_value(disp_val *v);
void disp_set_symbol_value(disp_val *v, disp_val *val);
disp_val* disp_make_int(int i);
disp_val* disp_make_long(long l);
char disp_get_byte(disp_val *v);
short disp_get_short(disp_val *v);
int disp_get_int(disp_val *v);
long disp_get_long(disp_val *v);
disp_val* disp_make_float(float f);
disp_val* disp_make_double(double d);
float disp_get_float(disp_val *v);
double disp_get_double(disp_val *v);
disp_val* disp_make_string(const char *s);
char* disp_get_str(disp_val *v);
size_t disp_get_str_len(disp_val *v);
disp_val* disp_define_type(char *name, disp_val *decl);
char* disp_get_type_name(disp_val *t);
disp_val* disp_get_type_decl(disp_val *t);
disp_builtin_t disp_get_builtin(disp_val *v);
char* disp_get_builtin_desc(disp_val *v);
disp_syscall_t disp_get_syscall(disp_val *v);
char* disp_get_syscall_desc(disp_val *v);
disp_val* disp_make_cons(disp_val *car, disp_val *cdr);
disp_val* disp_make_syscall(disp_syscall_t f, char *d);
disp_val* disp_make_builtin(disp_builtin_t f, char *d);
extern disp_val* disp_make_file(FILE *f, char *mode);
extern FILE* disp_get_file(disp_val *v);
extern char* disp_get_file_mode(disp_val *v);
extern void disp_set_file(disp_val *v, FILE *f);
extern void disp_set_file_mode(disp_val *v, char *mode);

/* --- Accessors (for cons cells) --- */
disp_val* disp_car(disp_val *cons);
disp_val* disp_cdr(disp_val *cons);
void disp_set_car(disp_val *cons, disp_val *car);   /* use with care – immutable by convention */
void disp_set_cdr(disp_val *cons, disp_val *cdr);

/* --- Symbol table --- */
disp_val* disp_find_symbol(const char *name);
disp_val* disp_define_symbol(const char *name, disp_val *value, int final);
void disp_remove_symbol(const char *name);
disp_val* disp_intern_symbol(const char *name);

/* --- numbers --- */
bool disp_is_float_literal(const char *s);
bool disp_is_double_literal(const char *s);
bool disp_is_long_literal(const char *s);
bool disp_is_integer_literal(const char *s);
disp_val* disp_parse_number(const char *s);

/* --- Evaluation --- */
disp_val* disp_eval(disp_val *expr);
disp_val* disp_eval_body(disp_val *body);

/* --- Garbage collection --- */
void disp_init_gc(void);
void disp_gc(void);
disp_val* disp_alloc(disp_flag_t flag, disp_data *data);
#define DISP_ALLOC(flag) disp_alloc(flag, gc_calloc(1, sizeof(disp_data)))
 

/* --- I/O --- */
disp_val* disp_read(FILE *f);
char* disp_string(disp_val *v);
char* disp_str(disp_val *v);
void disp_write(disp_val *v);
void disp_print(disp_val *v);
void disp_fprint(FILE *f, disp_val *v);
void disp_fwrite(FILE *f, disp_val *v);

/* --- Coroutine --- */
disp_val* disp_get_current_coro();
disp_val* disp_make_coroutine(disp_val *func, size_t stack_size);
void scheduler_add(disp_val *coro);
void scheduler_yield(void);
void scheduler_suspend(void);
void event_loop_add_fd(int fd, disp_val *coro, int events);
void event_loop_add_timer(long milliseconds, disp_val *coro);
disp_val* disp_make_socket(int fd);
int disp_get_socket_fd(disp_val *v);

/* --- Loading --- */
disp_val* disp_load(const char *filename);

/* --- Closures and macros --- */
disp_val* disp_make_closure(disp_val *params, disp_val *body);
disp_val* disp_make_macro(disp_val *params, disp_val *body);
disp_val* disp_get_closure_params(disp_val *closure);
disp_val* disp_get_closure_body(disp_val *closure);
disp_val* disp_apply_closure(disp_val *closure, disp_val **args, int arg_count);
disp_val* disp_expand_macro(disp_val *macro, disp_val *expr);

// disp_info_t 定义
typedef struct disp_info {
    char *filename;
    int line;                 // 当前表达式位置（用于错误输出）
    int column;
    int saved_line;           // 解析器保存的行号
    int saved_col;            // 解析器保存的列号
    struct disp_info *next;
} disp_info_t;

// 声明解析器当前行列号（定义在 info.c）
extern int parse_current_line;
extern int parse_current_col;
void disp_update_pos(int c);

// 线程局部信息管理（栈式）
void disp_init_info(void);                     // 初始化（确保 current_info 为 NULL）
void disp_push_source(const char *filename);   // 加载新文件时压栈
void disp_pop_source(void);                    // 文件加载完毕弹栈
void disp_update_current_pos(int line, int col); // 更新栈顶位置
disp_info_t* disp_get_current_info(void);      // 返回栈顶指针

/* --- disp.c (repl) --- */
void disp_init_globals(void);
void disp_repl(void);

/* --- Global constants --- */
extern disp_val *NIL, *TRUE, *QUIT;
extern disp_val *BYTE, *SHORT, *INT, *LONG, *FLOAT, *DOUBLE, *PNTR;
extern disp_val *LAMBDA, *LETA, *LETRECA;
extern disp_val *CONS, *LIST, *QUOTE, *QUASIQUOTE, *UNQUOTE, *UNQUOTE_SPLICING, *APPEND;
extern disp_val *MODPATH;

#endif /* DISP_H */
