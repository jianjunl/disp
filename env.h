
#ifndef SCOPE_H
#define SCOPE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* Opaque types */
typedef struct disp_env disp_env_t;

/* --- Scope/Environment --- */
void disp_init_env(void);
extern disp_env_t *disp_global_env;                     // 全局作用域指针
void gc_root_cleanup_disp_env_t(disp_env_t **ptr_addr);
disp_env_t* disp_new_env(disp_env_t *parent);
disp_val disp_find_symbol(const disp_env_t *env, disp_sid id);
disp_val disp_define_symbol(const disp_env_t *env, disp_sid id, disp_val value, int final);
disp_val disp_intern_symbol(const disp_env_t *env, disp_sid id);
disp_val disp_find_symbol_by_name(const disp_env_t *env, const char *name);
disp_val disp_define_symbol_by_name(const disp_env_t *env, const char *name, disp_val value, int final);
disp_val disp_intern_symbol_by_name(const disp_env_t *env, const char *name);
void disp_set_symbol_value(const disp_env_t *env, disp_val sym, disp_val value);
bool disp_update_symbol(const disp_env_t *env, disp_val sym);

#define SYMBOL_BY_NAME(env, name) disp_find_symbol_by_name(env, name)
#define SYMBOL(env, sid) disp_find_symbol(env, sid)
#define SET_THIS(env, type) disp_define_symbol(env, THIS, type, 1)
#define GET_THIS(env) disp_find_symbol(e, THIS)
#define GLOBAL(sid) disp_find_symbol(disp_global_env, sid)
#define REG(name, value, ro) disp_define_symbol_by_name(disp_global_env, name, value, ro)
#define REGI(sid, value, ro) disp_define_symbol(disp_global_env, sid, value, ro)
 
#endif //SCOPE_H
