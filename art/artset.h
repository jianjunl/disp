
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ASET_KLEN 8

typedef struct aset_conf {
    void* (*malloc)(size_t size);
    void* (*calloc)(size_t nmemb, size_t size);
    void  (*free)(void *ptr);
} aset_conf;

typedef enum {
    LEAF,
    NODE4,
    NODE16,
    NODE48,
    NODE256
} ASET_NODE;

typedef struct aset_node {
    union {
        struct { uint8_t keys[4]; struct aset_node *child[4]; } n4;
        struct { uint8_t keys[16]; struct aset_node *child[16]; } n16;
        struct { uint8_t index[256]; struct aset_node *child[48]; } n48;
        struct { struct aset_node *child[256]; } n256;
        struct { uint8_t key[ASET_KLEN]; } leaf;
    };
    uint32_t children;
    ASET_NODE type;
} aset_node;

typedef struct {
    aset_node *root;
    aset_conf *conf;
} ASET;

ASET* aset_create(aset_conf *conf);

void aset_insert(ASET *a, uint64_t k);
int aset_contains(ASET *a, uint64_t k);
int aset_delete(ASET *a, uint64_t k);
uint64_t aset_next(ASET *a, uint64_t key);
uint64_t aset_prev(ASET *a, uint64_t key);

int aset_shrink(ASET *a);
int aset_delete_with_shrink(ASET *a, uint64_t k);
void aset_destroy(ASET *a);
