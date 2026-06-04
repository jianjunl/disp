
#include "btree_private.h"
#include <stdlib.h>

void btree_borrow_from_left(btree_t *tree, btree_node_t *parent, int idx) {
    (void)tree;  // 消除警告
    btree_node_t *left = parent->children[idx - 1];
    btree_node_t *right = parent->children[idx];

    for (int i = right->n; i > 0; i--) {
        right->keys[i] = right->keys[i - 1];
        right->values[i] = right->values[i - 1];
    }
    if (!right->leaf) {
        for (int i = right->n + 1; i > 0; i--)
            right->children[i] = right->children[i - 1];
    }

    right->keys[0] = parent->keys[idx - 1];
    right->values[0] = parent->values[idx - 1];
    right->n++;

    parent->keys[idx - 1] = left->keys[left->n - 1];
    parent->values[idx - 1] = left->values[left->n - 1];
    left->n--;

    if (!right->leaf) {
        right->children[0] = left->children[left->n + 1];
        left->children[left->n + 1] = NULL;
    }
}

void btree_borrow_from_right(btree_t *tree, btree_node_t *parent, int idx) {
    (void)tree;
    btree_node_t *left = parent->children[idx];
    btree_node_t *right = parent->children[idx + 1];

    left->keys[left->n] = parent->keys[idx];
    left->values[left->n] = parent->values[idx];
    left->n++;

    parent->keys[idx] = right->keys[0];
    parent->values[idx] = right->values[0];

    for (int i = 0; i < right->n - 1; i++) {
        right->keys[i] = right->keys[i + 1];
        right->values[i] = right->values[i + 1];
    }

    if (!left->leaf) {
        left->children[left->n] = right->children[0];
        for (int i = 0; i < right->n; i++)
            right->children[i] = right->children[i + 1];
    }
    right->n--;
}

void btree_merge_children(btree_t *tree, btree_node_t *parent, int idx) {
    (void)tree;
    btree_node_t *left = parent->children[idx];
    btree_node_t *right = parent->children[idx + 1];

    left->keys[left->n] = parent->keys[idx];
    left->values[left->n] = parent->values[idx];
    left->n++;

    for (int i = 0; i < right->n; i++) {
        left->keys[left->n + i] = right->keys[i];
        left->values[left->n + i] = right->values[i];
    }
    if (!left->leaf) {
        for (int i = 0; i <= right->n; i++)
            left->children[left->n + i] = right->children[i];
    }
    left->n += right->n;

    for (int i = idx; i < parent->n - 1; i++) {
        parent->keys[i] = parent->keys[i + 1];
        parent->values[i] = parent->values[i + 1];
        parent->children[i + 1] = parent->children[i + 2];
    }
    parent->n--;

    free(right->keys);
    free(right->values);
    free(right->children);
    free(right);
}
