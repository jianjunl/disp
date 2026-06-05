#ifndef BTREE_PRIVATE_H
#define BTREE_PRIVATE_H

#include "btree.h"

// 节点结构
struct btree_node {
    uint64_t *keys;
    void **values;
    btree_node_t **children;
    int n;
    bool leaf;
};

// 基础函数
#if BTREE_NO_GC
btree_node_t* btree_node_create(int t, bool leaf);
void btree_node_destroy(btree_node_t *node, int t);
#else // BTREE_NO_GC
btree_node_t* btree_node_create_gc(int t, bool leaf);
#endif // BTREE_NO_GC

void* btree_search_node(const btree_node_t *node, uint64_t key, btree_cmp_t cmp, int t);  // 新增，供 delete.c 使用

// 插入相关
void btree_split_child(btree_t *tree, btree_node_t *parent, int idx);
void btree_insert_nonfull(btree_t *tree, btree_node_t *node, uint64_t key, void *value);

// 删除相关
void btree_merge_children(btree_t *tree, btree_node_t *parent, int idx);
void btree_borrow_from_left(btree_t *tree, btree_node_t *parent, int idx);
void btree_borrow_from_right(btree_t *tree, btree_node_t *parent, int idx);
void btree_delete_node(btree_t *tree, btree_node_t *node, uint64_t key);  // 可选，内部递归

#endif
