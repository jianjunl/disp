
#include <stddef.h>
#include <stdio.h>

#include "test.h"

static inline void gc_auto_root_cleanup(void **ptr) {
    if (ptr && *ptr) {
        printf("gc_auto_root_cleanup: %p\n", *ptr);
        gc_remove_root(*ptr);
    }
}

static const char *globalstr = "thisisateststring";

int main() {
    GC_ROOT_AUTO(globalstr);
    return 0;
}
