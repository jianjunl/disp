/* test_types.c - Compile-time validation of typed allocation macros.
 * Compile: gcc -std=c11 -Wall -Wextra -c test_types.c
 * No linking required. */

#include <stddef.h>
#include <stdio.h>
#include <ctype.h>

typedef struct gc_type_info {
    size_t  object_size;
    size_t  n_offsets;
    ssize_t offsets[];          /* flexible array, n_offsets elements */
} gc_type_info_t;

/* Dummy implementations just to avoid linker errors (if you really want to link,
   remove them and link with the real gc library). */
void *gc_malloc(size_t size)   { (void)size; return 0; }
void *gc_calloc(size_t n, size_t s) { (void)n; (void)s; return 0; }
void* gc_typed_malloc(size_t size, const gc_type_info_t *ti) { (void)size; (void)ti; return 0; }
void* gc_typed_calloc(size_t n, size_t s, const gc_type_info_t *ti) { (void)n; (void)s; (void)ti; return 0; }

#define GC_TYPE_INFO(name, struct_type, ...)                          \
    static const gc_type_info_t name = {                              \
        .object_size = sizeof(struct_type),                           \
        .n_offsets   = (sizeof((ssize_t[]){ __VA_ARGS__ }) /          \
                        sizeof(ssize_t)),                             \
        .offsets     = { __VA_ARGS__ }                                \
    }

#define GC_TYPE_BEGIN(name, struct_type)                              \
    static ssize_t name ## _offsets[] = {
#define GC_FIELD(struct_type, member) offsetof(struct_type, member),
#define GC_TYPE_END(name, struct_type)                                \
    };                                                                \
    static const gc_type_info_t name = {                              \
        .object_size = sizeof(struct_type),                           \
        .n_offsets   = (sizeof(name ## _offsets) /                    \
                        sizeof(name ## _offsets[0])),                 \
        .offsets     = name ## _offsets                               \
    };

#define GC_GLOBAL(type, name) \
    type * name __attribute__((section("gc_roots"))) = NULL
#define GC_GLOBAL1(type, x)      GC_GLOBAL(type, x)
#define GC_GLOBAL2(type, x, ...) GC_GLOBAL(type, x);GC_GLOBAL1(type, __VA_ARGS__)
#define GC_GLOBAL3(type, x, ...) GC_GLOBAL(type, x);GC_GLOBAL2(type, __VA_ARGS__)
#define GC_GLOBAL4(type, x, ...) GC_GLOBAL(type, x);GC_GLOBAL3(type, __VA_ARGS__)
#define GC_GLOBAL5(type, x, ...) GC_GLOBAL(type, x);GC_GLOBAL4(type, __VA_ARGS__)
#define GC_GLOBAL6(type, x, ...) GC_GLOBAL(type, x);GC_GLOBAL5(type, __VA_ARGS__)
#define GC_GLOBAL7(type, x, ...) GC_GLOBAL(type, x);GC_GLOBAL6(type, __VA_ARGS__)
#define GC_GLOBAL8(type, x, ...) GC_GLOBAL(type, x);GC_GLOBAL7(type, __VA_ARGS__)
#define GC_GLOBAL9(type, x, ...) GC_GLOBAL(type, x);GC_GLOBAL8(type, __VA_ARGS__)


/* ---------------------- Example structures ---------------------- */

struct list_node {
    int value;
    struct list_node *next;
    struct list_node *prev;
};

typedef struct {
    void *data;
    size_t size;
} buffer_t;

union ptr_or_int {
    void *ptr;
    int   num;
};

struct complex {
    int kind;
    union ptr_or_int u;
    struct list_node *extra;
    buffer_t *buf;
};

/* ------------------ Type descriptors with GC_TYPE_INFO (preferred) ------- */

GC_TYPE_INFO(list_type, struct list_node,
             offsetof(struct list_node, next),
             offsetof(struct list_node, prev));

GC_TYPE_INFO(buffer_type, buffer_t,
             offsetof(buffer_t, data));

/* For the union, list every possible pointer offset */
GC_TYPE_INFO(union_type, union ptr_or_int,
             offsetof(union ptr_or_int, ptr));

GC_TYPE_INFO(complex_type, struct complex,
             offsetof(struct complex, u.ptr),   /* pointer inside union */
             offsetof(struct complex, extra),
             offsetof(struct complex, buf));

/* ------------------ Same descriptors using BEGIN/END style --------------- */

GC_TYPE_BEGIN(list_type2, struct list_node)
    GC_FIELD(struct list_node, next)
    GC_FIELD(struct list_node, prev)
GC_TYPE_END(list_type2, struct list_node)

GC_TYPE_BEGIN(buffer_type2, buffer_t)
    GC_FIELD(buffer_t, data)
GC_TYPE_END(buffer_type2, buffer_t)

GC_TYPE_BEGIN(union_type2, union ptr_or_int)
    GC_FIELD(union ptr_or_int, ptr)
GC_TYPE_END(union_type2, union ptr_or_int)

GC_TYPE_BEGIN(complex_type2, struct complex)
    GC_FIELD(struct complex, u.ptr)
    GC_FIELD(struct complex, extra)
    GC_FIELD(struct complex, buf)
GC_TYPE_END(complex_type2, struct complex)

/* ------------------ Global root variables ----------------------- */

GC_GLOBAL(struct list_node, global_list);
GC_GLOBAL(buffer_t, global_buffer);

/* Variadic forms */
GC_GLOBAL2(struct list_node, list_a, list_b);
GC_GLOBAL3(buffer_t, buf_x, buf_y, buf_z);

/* ------------------ Function that uses typed allocation --------- */

void test_allocations(void) {
    struct list_node *n1 = gc_typed_malloc(sizeof(struct list_node), &list_type);
    struct list_node *n2 = gc_typed_calloc(1, sizeof(struct list_node), &list_type);

    buffer_t *buf = gc_typed_malloc(sizeof(buffer_t), &buffer_type);

    /* Array allocation */
    struct list_node *arr = gc_typed_malloc(10 * sizeof(struct list_node), &list_type);

    /* Complex struct */
    struct complex *c = gc_typed_malloc(sizeof(struct complex), &complex_type);

    /* Prevent unused-variable warnings */
    (void)n1; (void)n2; (void)buf; (void)arr; (void)c;
}
