
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
disp_env_t* disp_dup_env(disp_env_t *old);
disp_val disp_find_symbol(const disp_env_t *env, uint64_t id);
disp_val disp_define_symbol(const disp_env_t *env, uint64_t id, disp_val value, int final);
disp_val disp_intern_symbol(const disp_env_t *env, uint64_t id);
disp_val disp_find_symbol_by_name(const disp_env_t *env, const char *name);
disp_val disp_define_symbol_by_name(const disp_env_t *env, const char *name, disp_val value, int final);
disp_val disp_intern_symbol_by_name(const disp_env_t *env, const char *name);

#define DEF(n, v, i) disp_define_symbol_by_name(disp_global_env, n, v, i)
 
#endif //SCOPE_H
