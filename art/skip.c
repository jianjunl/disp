
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "skip.h"

#define MAX_LEVEL 16
#define P_FACTOR 0.5

typedef struct skip_node {
    uintptr_t value;
    struct skip_node **forward;
} skip_node;

struct skip_list {
    skip_conf *conf;
    skip_node *head;
    int level;
};

static skip_node* create_node(skip_list *sl, uintptr_t value, int level) {
    skip_node *node = (skip_node*)sl->conf->malloc(sizeof(skip_node));
    node->value = value;
    node->forward = (skip_node**)sl->conf->malloc(sizeof(skip_node*) * level);
    for (int i = 0; i < level; i++) {
        node->forward[i] = NULL;
    }
    return node;
}

// 默认比较函数（数值比较）
static inline int default_cmp(uintptr_t a, uintptr_t b) {
    return a < b ? -1 : a > b ? 1 : 0;
}

static skip_conf default_conf = (skip_conf) {
    .cmp    = default_cmp,
    .malloc = malloc,
    .calloc = calloc,
    .free   = free
}; 

skip_list* skip_create(skip_conf *c) {
    if (!c) c = &default_conf;
    if (!c->cmp) c->cmp = default_cmp;
    if (!c->malloc) c->malloc = malloc;
    if (!c->calloc) c->calloc = calloc;
    if (!c->free) c->free = free;
    skip_list *sl = (skip_list*)c->malloc(sizeof(skip_list));
    if (!sl) return NULL;
    sl->conf = c;
    sl->head = create_node(sl, -1, MAX_LEVEL);
    sl->level = 0;
    srand(time(NULL));
    return sl;
}

static int random_level() {
    int level = 1;
    while ((rand() & 0xffff) < (P_FACTOR * 0xffff) && level < MAX_LEVEL) {
        level++;
    }
    return level;
}

bool skip_search(skip_list *sl, uintptr_t target) {
    skip_node *curr = sl->head;
    for (int i = sl->level - 1; i >= 0; i--) {
        while (curr->forward[i] && sl->conf->cmp(curr->forward[i]->value, target) == -1) {
            curr = curr->forward[i];
        }
    }
    curr = curr->forward[0];
    return curr && sl->conf->cmp(curr->value, target) == 0;
}

void skip_insert(skip_list *sl, uintptr_t value) {
    skip_node *update[MAX_LEVEL];
    skip_node *curr = sl->head;
    for (int i = sl->level - 1; i >= 0; i--) {
        while (curr->forward[i] && sl->conf->cmp(curr->forward[i]->value, value) == -1) {
            curr = curr->forward[i];
        }
        update[i] = curr;
    }
    int lvl = random_level();
    if (lvl > sl->level) {
        for (int i = sl->level; i < lvl; i++) {
            update[i] = sl->head;
        }
        sl->level = lvl;
    }
    skip_node *new_node = create_node(sl, value, lvl);
    for (int i = 0; i < lvl; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }
}

bool skip_delete(skip_list *sl, uintptr_t value) {
    skip_node *update[MAX_LEVEL];
    skip_node *curr = sl->head;
    for (int i = sl->level - 1; i >= 0; i--) {
        while (curr->forward[i] && sl->conf->cmp(curr->forward[i]->value, value) == -1) {
            curr = curr->forward[i];
        }
        update[i] = curr;
    }
    curr = curr->forward[0];
    if (!curr || sl->conf->cmp(curr->value, value) != 0) {
        return false;
    }
    for (int i = 0; i < sl->level; i++) {
        if (update[i]->forward[i] != curr) {
            break;
        }
        update[i]->forward[i] = curr->forward[i];
    }
    sl->conf->free(curr->forward);
    sl->conf->free(curr);
    while (sl->level > 0 && sl->head->forward[sl->level - 1] == NULL) {
        sl->level--;
    }
    return true;
}

void skip_destroy(skip_list *sl) {
    skip_node *curr = sl->head;
    while (curr) {
        skip_node *temp = curr;
        curr = curr->forward[0];
        sl->conf->free(temp->forward);
        sl->conf->free(temp);
    }
    skip_conf *c = sl->conf;
    c->free(sl);
}
