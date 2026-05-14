
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdatomic.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
//#define DEBUG 1
#include "disp.h"

/* ======================== Load Function ======================== */

extern void disp_push_source(const char *filename);
extern void disp_pop_source(void);
extern disp_scope_t *global_scope;

static disp_val* load_lisp(disp_scope_t *env, const char *filename) {
    if (!env) env = global_scope;
    if (!strchr(filename, '/')) {
        char fn[PATH_MAX]; 
        strcpy(fn, disp_get_str(MODPATH));
   	filename = strcat(fn, filename); 
    }
    FILE *f = fopen(filename, "r");
    if (!f) { perror("fopen"); return NIL; }
    // 压入新文件信息
    disp_push_source(filename);
    disp_val *last = NIL;
    disp_val *expr;
    while ((expr = disp_read(f)) != NULL) {
        last = disp_eval(env, expr);
    }
    fclose(f);
    // 弹栈恢复外层
    disp_pop_source();
    return last;
}

static disp_val* load_so(const char *filename) {
    if (!strchr(filename, '/')) {
        char fn[PATH_MAX]; 
        strcpy(fn, disp_get_str(MODPATH));
   	filename = strcat(fn, filename); 
    }
    DBG("load_so: attempting to load %s\n", filename);
    //void *handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
    //void *handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
    void *handle = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);   // 改为 GLOBAL
    //void *handle = dlopen(filename, RTLD_LAZY | RTLD_GLOBAL);   // 改为 GLOBAL
    if (!handle) {
        ERET(NIL, "dlopen: %s %s\n", filename, dlerror());
    }
    DBG("dlopen succeeded\n");
    void (*init)(void) = dlsym(handle, "disp_init_module");
    if (!init) {
        dlclose(handle);
        ERET(NIL, "dlsym: %s\n", dlerror());
    }
    DBG("dlsym succeeded, calling init...\n");
    init();
    DBG("init returned\n");
    return TRUE;
}

disp_val* disp_load(disp_scope_t *env, const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext && strcmp(ext, ".lisp") == 0)
        return load_lisp(env, filename);
    if (ext && strcmp(ext, ".disp") == 0)
        return load_lisp(env, filename);
    ERET(NIL, "unknown extension: %s\n", filename);
}

disp_val* disp_import(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext && strcmp(ext, ".so") == 0)
        return load_so(filename);
    if (ext && strcmp(ext, ".lisp") == 0)
        return load_lisp(global_scope, filename);
    if (ext && strcmp(ext, ".disp") == 0)
        return load_lisp(global_scope, filename);
    ERET(NIL, "unknown extension: %s\n", filename);
}
