
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// 比较函数：返回 -1 表示 a < b, 0 表示相等, 1 表示 a > b
typedef int (*skip_cmp)(uintptr_t a, uintptr_t b);

typedef struct skip_conf {
    skip_cmp cmp;
    void* (*malloc)(size_t size);
    void* (*calloc)(size_t nmemb, size_t size);
    void  (*free)(void *ptr);
} skip_conf;

typedef struct skip_list skip_list;

skip_list* skip_create(skip_conf *conf);

bool skip_search(skip_list *sl, uintptr_t target);

void skip_insert(skip_list *sl, uintptr_t value);

bool skip_delete(skip_list *sl, uintptr_t value);

void skip_destroy(skip_list *sl);
