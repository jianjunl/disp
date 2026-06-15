
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define AMAP_KLEN 8

typedef struct amap_conf {
    void* (*malloc)(size_t size);
    void* (*calloc)(size_t nmemb, size_t size);
    void  (*free)(void *ptr);
} amap_conf;

typedef enum {
    LEAF,
    NODE4,
    NODE16,
    NODE48,
    NODE256
} AMAP_NODE;

typedef struct amap_node {
    union {
        struct { uint8_t keys[4]; struct amap_node *child[4]; } n4;
        struct { uint8_t keys[16]; struct amap_node *child[16]; } n16;
        struct { uint8_t index[256]; struct amap_node *child[48]; } n48;
        struct { struct amap_node *child[256]; } n256;
        struct { uint8_t key[AMAP_KLEN]; void *val; } leaf;
    };
    uint32_t children;
    AMAP_NODE type;
} amap_node;

typedef struct {
    amap_node *root;
    amap_conf *conf;
} AMAP;

AMAP* amap_create(amap_conf *conf);

void amap_insert(AMAP *a, uint64_t k, void *v);

void *amap_search(AMAP *a, uint64_t k);

int amap_delete(AMAP *a, uint64_t k);

uint64_t amap_next(AMAP *a, uint64_t k, void **out_val);
uint64_t amap_prev(AMAP *a, uint64_t k, void **out_val);

int amap_shrink(AMAP *a);
int amap_delete_with_shrink(AMAP *a, uint64_t k);
void amap_destroy(AMAP *a);
