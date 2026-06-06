
#include "gc/gc.h"
#include "disp.h"

typedef disp_sid btree_key_t;
typedef disp_val btree_val_t;

#define BT_MALLOC(n) gc_typed_malloc(n, &GC_TYPE_PTR_ARRAY)
#define BT_FREE(v)   gc_free(v)
#define VNULL DNULL
#define KNULL SNULL

static inline int id_cmp(disp_sid a, disp_sid b) {
    return (a.id < b.id) ? -1 : (a.id > b.id) ? 1 : 0;
}
