
#include "gc/gc.h"
#include "disp.h"

typedef uint64_t btree_key_t;
typedef disp_val btree_val_t;

#define BT_MALLOC(n) gc_typed_malloc(n, &GC_TYPE_PTR_ARRAY)
#define BT_FREE(v)   gc_free(v)
#define VNULL DNULL
