
#ifndef SCOPE_H
#define SCOPE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* Opaque types */
typedef struct disp_scope disp_scope_t;

/* --- Scope/Environment --- */
void disp_init_scope(void);
extern disp_scope_t *disp_global_scope;                     // 全局作用域指针
void gc_root_cleanup_disp_scope_t(disp_scope_t **ptr_addr);
disp_scope_t* disp_new_scope(disp_scope_t *parent);
disp_scope_t* disp_dup_scope(disp_scope_t *old);
disp_box disp_find_symbol_by_id(const disp_scope_t *scope, uint64_t id);
disp_box disp_define_symbol_by_id(const disp_scope_t *scope, uint64_t id, disp_box value, int final);
disp_box disp_intern_symbol_by_id(const disp_scope_t *scope, uint64_t id);
disp_box disp_find_symbol(const disp_scope_t *scope, const char *name);
disp_box disp_define_symbol(const disp_scope_t *scope, const char *name, disp_box value, int final);
disp_box disp_intern_symbol(const disp_scope_t *scope, const char *name);

#define DEF(n, v, i) disp_define_symbol(NULL, n, v, i)
 
#endif //SCOPE_H
