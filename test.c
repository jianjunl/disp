#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h> // For ssize_t

typedef struct gc_type_info {
    size_t  object_size;
    size_t  n_offsets;
    const ssize_t *offsets; // Changed from ssize_t offsets[] to a pointer
} gc_type_info_t;

void* gc_typed_malloc(size_t size, const gc_type_info_t *ti) { (void)size; (void)ti; return 0; }

#define GC_TYPE_BEGIN(name, struct_type)                                \
    static const ssize_t name ## _offsets[] = {

#define GC_FIELD(struct_type, member) offsetof(struct_type, member),

#define GC_TYPE_END(name, struct_type)                                  \
    };                                                                  \
    static const gc_type_info_t name = {                                \
        .object_size = sizeof(struct_type),                             \
        .n_offsets   = (sizeof(name ## _offsets) /                      \
                        sizeof(name ## _offsets[0])),                   \
        .offsets     = name ## _offsets                                 \
    };

struct list_node {
    int value;
    struct list_node *next;
    struct list_node *prev;
};

GC_TYPE_BEGIN(list_type, struct list_node)
    GC_FIELD(struct list_node, next)
    GC_FIELD(struct list_node, prev)
GC_TYPE_END(list_type, struct list_node)

int main() {
    struct list_node *n1 = gc_typed_malloc(sizeof(struct list_node), &list_type);
    return n1 ? 0 : 1;
}
