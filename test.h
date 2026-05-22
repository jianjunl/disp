
static inline void gc_add_root(void *ptr) {
    if (!ptr) return;
    printf("Trying to ADD %p\n", (void**)ptr);
}

static inline void gc_remove_root(void *ptr) {
    if (!ptr) return;
    printf("Trying to REMOVE %p\n", (void**)ptr);
}

#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)

#define GC_ROOT_AUTO(var) \
    void* CONCAT(__gc_auto_root_, __COUNTER__) __attribute__((cleanup(gc_auto_root_cleanup))) = (void*)&(var); \
    gc_add_root(&(var))
