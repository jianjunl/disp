#include "btree_private.h"
#include <stdlib.h>
#include <string.h>

// 分裂父节点的第 idx 个子节点（该子节点已满）
void btree_split_child(btree_t *tree, btree_node_t *parent, int idx) {
    int t = tree->t;
    btree_node_t *child = parent->children[idx];
#if BTREE_NO_GC
    btree_node_t *new_child = btree_node_create(t, child->leaf);
#else // BTREE_NO_GC
    btree_node_t *new_child = btree_node_create_gc(t, child->leaf);
#endif // BTREE_NO_GC
    
    if (!new_child) {
        // 内存分配失败处理（实际可根据需要 abort 或 return）
        return;
    }

    // 新节点获得原节点的后 t-1 个键
    new_child->n = t - 1;
    for (int i = 0; i < t-1; i++) {
        new_child->keys[i] = child->keys[i + t];
        new_child->values[i] = child->values[i + t];
    }
    // 如果不是叶子，还要复制子节点指针
    if (!child->leaf) {
        for (int i = 0; i < t; i++) {
            new_child->children[i] = child->children[i + t];
        }
    }
    
    // 原节点保留前 t-1 个键
    child->n = t - 1;
    
    // 在父节点中插入新子节点指针
    for (int i = parent->n; i > idx; i--) {
        parent->children[i+1] = parent->children[i];
    }
    parent->children[idx+1] = new_child;
    
    // 将 child 的中间键提升到父节点
    for (int i = parent->n-1; i >= idx; i--) {
        parent->keys[i+1] = parent->keys[i];
        parent->values[i+1] = parent->values[i];
    }
    parent->keys[idx] = child->keys[t-1];
    parent->values[idx] = child->values[t-1];
    parent->n++;
}
