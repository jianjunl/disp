
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
#include "array.h"

#include "box.h"

/* Opaque types */
typedef struct disp_scope disp_scope_t;

/* Function pointer type for built‑in functions */
typedef disp_box (*disp_syscall_t)(disp_box * args, int arg_count);
typedef disp_box (*disp_builtin_t)(disp_scope_t *scope, disp_box arg);

/* --- Value constructors --- */
void disp_init_name_table(void);
void disp_init_cons_pool(void);
uint64_t disp_get_id(const char *name);
const char* disp_get_name(uint64_t id);
void disp_lock_table(void);
void disp_unlock_table(void);
disp_box disp_make_symbol(const char *name);
char* disp_get_symbol_name(disp_box v);
disp_box disp_get_symbol_value(disp_box v);
disp_box disp_make_int(int i);
disp_box disp_make_long(long l);
char disp_get_byte(disp_box v);
short disp_get_short(disp_box v);
int disp_get_int(disp_box v);
long disp_get_long(disp_box v);
disp_box disp_make_float(float f);
disp_box disp_make_double(double d);
float disp_get_float(disp_box v);
double disp_get_double(disp_box v);
disp_box disp_make_string(const char *s);
char* disp_get_str(disp_box v);
size_t disp_get_str_len(disp_box v);
disp_box disp_define_type(char *name, disp_box decl);
char* disp_get_type_name(disp_box t);
disp_box disp_get_type_decl(disp_box t);
disp_builtin_t disp_get_builtin(disp_box v);
char* disp_get_builtin_desc(disp_box v);
disp_syscall_t disp_get_syscall(disp_box v);
char* disp_get_syscall_desc(disp_box v);
disp_box disp_make_cons(disp_box car, disp_box cdr);
disp_box disp_make_syscall(disp_syscall_t f, char *d);
disp_box disp_make_builtin(disp_builtin_t f, char *d);
extern disp_box disp_make_file(FILE *f, char *mode);
extern FILE* disp_get_file(disp_box v);
extern char* disp_get_file_mode(disp_box v);
extern void disp_set_file(disp_box v, FILE *f);
extern void disp_set_file_mode(disp_box v, char *mode);

/* --- Accessors (for cons cells) --- */
disp_box disp_car(disp_box cons);
disp_box disp_cdr(disp_box cons);
void disp_set_car(disp_box cons, disp_box car);   /* use with care – immutable by convention */
void disp_set_cdr(disp_box cons, disp_box cdr);

/* --- Scope/Environment --- */
void disp_init_scope(void);
extern disp_scope_t *disp_global_scope;                     // 全局作用域指针
void gc_root_cleanup_disp_scope_t(disp_scope_t **ptr_addr);
disp_scope_t* disp_new_scope(disp_scope_t *parent);
disp_scope_t* disp_dup_scope(disp_scope_t *old);

/* --- Symbol table --- */
void disp_init_symbol(void);
disp_box disp_find_symbol_by_id(const disp_scope_t *scope, uint64_t id);
disp_box disp_define_symbol_by_id(const disp_scope_t *scope, uint64_t id, disp_box value, int final);
disp_box disp_intern_symbol_by_id(const disp_scope_t *scope, uint64_t id);
disp_box disp_find_symbol(const disp_scope_t *scope, const char *name);
disp_box disp_define_symbol(const disp_scope_t *scope, const char *name, disp_box value, int final);
disp_box disp_intern_symbol(const disp_scope_t *scope, const char *name);
void disp_set_symbol_value(disp_box sym, disp_box value);

/* --- numbers --- */
bool disp_is_float_literal(const char *s);
bool disp_is_double_literal(const char *s);
bool disp_is_long_literal(const char *s);
bool disp_is_integer_literal(const char *s);
disp_box disp_parse_number(const char *s);

/* --- Evaluation --- */
disp_box disp_eval(disp_scope_t *scope, disp_box expr);
disp_box disp_eval_body(disp_scope_t *scope, disp_box body);
disp_scope_t* disp_get_closure_env(disp_box closure);

/* --- Allocation --- */
disp_box disp_alloc(disp_flag_t flag, disp_data *data);
#define DISP_ALLOC_TI(flag) disp_alloc(flag, gc_typed_calloc(1, sizeof(union disp_data), &union_disp_data_ti))
#define DISP_ALLOC(flag) disp_alloc(flag, gc_calloc(1, sizeof(union disp_data)))
 

/* --- I/O --- */
disp_box disp_read(FILE *f);
char* disp_string(disp_box v);
char* disp_str(disp_box v);
void disp_write(disp_box v);
void disp_print(disp_box v);
void disp_fprint(FILE *f, disp_box v);
void disp_fwrite(FILE *f, disp_box v);

/* --- Coroutine --- */
disp_box disp_get_current_coro();
disp_box disp_make_coroutine(disp_box func, size_t stack_size);
void scheduler_add(disp_box coro);
void scheduler_yield(void);
void scheduler_suspend(void);
void event_loop_add_fd(int fd, disp_box coro, int events);
void event_loop_add_timer(long milliseconds, disp_box coro);
disp_box disp_make_socket(int fd);
int disp_get_socket_fd(disp_box v);

/* --- Loading --- */
disp_box disp_import(const char *filename);
disp_box disp_load(disp_scope_t *env, const char *filename);

/* --- Closures and macros --- */
void disp_closure_reuse(disp_box closure);
disp_box disp_make_closure(disp_scope_t *env, disp_box params, disp_box body, int reuse_scope);
disp_box disp_make_macro(disp_scope_t *env, disp_box params, disp_box body, int reuse_scope);
disp_box disp_get_closure_params(disp_box closure);
disp_box disp_get_closure_body(disp_box closure);
disp_box disp_apply_closure(disp_box closure, disp_box *args, int arg_count);
disp_box disp_letf(disp_scope_t *scope, disp_box expr);
void bind_arguments_to_scope(disp_scope_t *scope, disp_box params, disp_box *args, int arg_count);

// disp_info_t 定义
typedef struct disp_info {
    char *filename;
    int line;                 // 当前表达式位置（用于错误输出）
    int column;
    int saved_line;           // 解析器保存的行号
    int saved_col;            // 解析器保存的列号
    struct disp_info *next;
} disp_info_t;

// 线程局部信息管理（栈式）
void disp_init_info(void);                     // 初始化（确保 current_info 为 NULL）
disp_info_t* disp_get_current_info(void);      // 返回栈顶指针

/* --- disp.c (repl) --- */
void disp_init_globals(void);
void disp_repl(void);

extern disp_box disp_builtin_roots[];

#define NUM_BUILTIN_ROOTS 33

enum disp_builtin_root_idx {
    IDX_NIL = 0, IDX_TRUE, IDX_QUIT,
    IDX_BYTE, IDX_SHORT, IDX_INT, IDX_LONG, IDX_FLOAT, IDX_DOUBLE, IDX_PNTR,
    IDX_LAMBDA, IDX_LET, IDX_LETA, IDX_LETREC, IDX_LETRECA,
    IDX_CONS, IDX_LIST, IDX_QUOTE, IDX_QUASIQUOTE, IDX_UNQUOTE, IDX_UNQUOTE_SPLICING, IDX_APPEND,
    IDX_ELSE, IDX_IF, IDX_BEGIN, IDX_PROGN, IDX_COND, IDX_AND, IDX_OR, IDX_SET, IDX_SETQ, IDX_DEFINE,
    IDX_MODPATH
};

#define NIL              disp_builtin_roots[IDX_NIL]
#define TRUE             disp_builtin_roots[IDX_TRUE]
#define QUIT             disp_builtin_roots[IDX_QUIT]
#define BYTE             disp_builtin_roots[IDX_BYTE]
#define SHORT            disp_builtin_roots[IDX_SHORT]
#define INT              disp_builtin_roots[IDX_INT]
#define LONG             disp_builtin_roots[IDX_LONG]
#define FLOAT            disp_builtin_roots[IDX_FLOAT]
#define LONG             disp_builtin_roots[IDX_LONG]
#define DOUBLE           disp_builtin_roots[IDX_DOUBLE]
#define PNTR             disp_builtin_roots[IDX_PNTR]
#define LAMBDA           disp_builtin_roots[IDX_LAMBDA]
#define LET              disp_builtin_roots[IDX_LET]
#define LETA             disp_builtin_roots[IDX_LETA]
#define LETREC           disp_builtin_roots[IDX_LETREC]
#define LETRECA          disp_builtin_roots[IDX_LETRECA]
#define CONS             disp_builtin_roots[IDX_CONS]
#define LIST             disp_builtin_roots[IDX_LIST]
#define QUOTE            disp_builtin_roots[IDX_QUOTE]
#define QUASIQUOTE       disp_builtin_roots[IDX_QUASIQUOTE]
#define UNQUOTE          disp_builtin_roots[IDX_UNQUOTE]
#define UNQUOTE_SPLICING disp_builtin_roots[IDX_UNQUOTE_SPLICING]
#define APPEND           disp_builtin_roots[IDX_APPEND]
#define ELSE             disp_builtin_roots[IDX_ELSE]
#define IF               disp_builtin_roots[IDX_IF]
#define BEGIN            disp_builtin_roots[IDX_BEGIN]
#define PROGN            disp_builtin_roots[IDX_PROGN]
#define COND             disp_builtin_roots[IDX_COND]
#define AND              disp_builtin_roots[IDX_AND]
#define OR               disp_builtin_roots[IDX_OR]
#define SET              disp_builtin_roots[IDX_SET]
#define SETQ             disp_builtin_roots[IDX_SETQ]
#define DEFINE           disp_builtin_roots[IDX_DEFINE]
#define MODPATH          disp_builtin_roots[IDX_MODPATH]

#endif /* DISP_H */
