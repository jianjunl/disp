
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "disp.h"

#if DISP_BOXING

inline disp_env_t* disp_get_scope_env(disp_val v) {
    if (N(v) || E(v, NIL)) return NULL;
    if (T(v) != FLAG_SCOPE) {
	ERRO("disp_get_scope_env failed");
        return NULL;
    }
    return (disp_env_t *)BOX_UNBOX(v);
}

inline disp_val disp_new_scope(disp_val parent) {
    disp_env_t *e = disp_new_env(disp_get_scope_env(parent));
    return BOX_BOX(FLAG_SCOPE, e);
}

#else // DISP_BOXING

inline disp_env_t* disp_get_scope_env(disp_val v) {
    if (N(v) || E(v, NIL)) return NULL;
    if (T(v) != FLAG_SCOPE) {
	ERRO("disp_get_scope_env failed");
        return NULL;
    }
    return (disp_env_t *)v.x;
}

inline disp_val disp_new_scope(disp_val parent) {
    disp_env_t *e = disp_new_env(disp_get_scope_env(parent));
    return (disp_val){.flag = FLAG_SCOPE, .x = (uint64_t)e};
}

#endif // DISP_BOXING
