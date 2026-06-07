#ifndef BTREE_PRIVATE_H
#define BTREE_PRIVATE_H

#include "btree.h"

// 节点结构
struct bt_node {
    bt_key_t   *keys;
    bt_val_t   *values;
    bt_node_t **children;
    int n;
    bool leaf;
};

// 基础函数
bt_node_t* bt_node_create(int t, bool leaf);
void bt_node_destroy(bt_node_t *node, int t);

bt_val_t btree_search_node(const bt_node_t *node, bt_key_t key, bt_cmp_t cmp, int t);  // 新增，供 delete.c 使用

// 插入相关
void btree_split_child(btree_t *tree, bt_node_t *parent, int idx);
void btree_insert_nonfull(btree_t *tree, bt_node_t *node, bt_key_t key, bt_val_t value);

// 删除相关
void btree_merge_children(btree_t *tree, bt_node_t *parent, int idx);
void btree_borrow_from_left(btree_t *tree, bt_node_t *parent, int idx);
void btree_borrow_from_right(btree_t *tree, bt_node_t *parent, int idx);
void btree_delete_node(btree_t *tree, bt_node_t *node, bt_key_t key);  // 可选，内部递归

#endif
