
#include "btree.h"
#include "btree_private.h"   // 确保声明一致
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if BTREE_DEFAULT

// 默认比较函数（数值比较）
static int default_cmp(btree_key_t a, btree_key_t b) {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

#endif // BTREE_DEFAULT

// 释放节点（仅内部使用，保留 static）
void btree_node_destroy(btree_node_t *node, int t) {
    if (!node) return;
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            btree_node_destroy(node->children[i], t);
        }
    }
    BT_FREE(node->keys);
    BT_FREE(node->values);
    BT_FREE(node->children);
    BT_FREE(node);
}

// 销毁B树
void btree_destroy(btree_t *tree) {
    if (!tree) return;
    if (tree->root)
        btree_node_destroy(tree->root, tree->t);
    BT_FREE(tree);
}

// 创建B树
btree_t* btree_create(int t, btree_cmp_t cmp) {
    if (t < 2) t = 2;   // 最小度数至少为2
    btree_t *tree = (btree_t*)BT_MALLOC(sizeof(btree_t));
    if (!tree) return NULL;
    tree->t = t;
#if BTREE_DEFAULT
    tree->cmp = cmp ? cmp : default_cmp;
#else  // BTREE_DEFAULT
    tree->cmp = cmp;
#endif // BTREE_DEFAULT
    // 创建根节点（初始为叶子）
    tree->root = btree_node_create(t, true);
    if (!tree->root) {
        BT_FREE(tree);
        return NULL;
    }
    return tree;
}

// 创建新节点（非静态，供其他模块使用）
btree_node_t* btree_node_create(int t, bool leaf) {
    btree_node_t *node = (btree_node_t*)BT_MALLOC(sizeof(btree_node_t));
    if (!node) return NULL;
    node->n = 0;
    node->leaf = leaf;
    node->keys = (btree_key_t*)BT_MALLOC((2*t - 1) * sizeof(btree_key_t));
    node->values = (btree_val_t*)BT_MALLOC((2*t - 1) * sizeof(btree_val_t));
    node->children = (btree_node_t**)BT_MALLOC((2*t) * sizeof(btree_node_t*));
    if (!node->keys || !node->values || !node->children) {
        BT_FREE(node->keys); BT_FREE(node->values); BT_FREE(node->children); BT_FREE(node);
        return NULL;
    }
    for (int i = 0; i < 2*t; i++) {
        if (i < 2*t-1) {
            node->keys[i] = KNULL;
            node->values[i] = VNULL;
        }
        node->children[i] = NULL;
    }
    return node;
}

// 查找键（非静态，供 delete.c 使用）
btree_val_t btree_search_node(const btree_node_t *node, btree_key_t key, btree_cmp_t cmp, int t) {
    int i = 0;
    while (i < node->n && cmp(key, node->keys[i]) > 0)
        i++;
    if (i < node->n && cmp(key, node->keys[i]) == 0)
        return node->values[i];
    if (node->leaf)
        return VNULL;
    return btree_search_node(node->children[i], key, cmp, t);
}

btree_val_t btree_search(const btree_t *tree, btree_key_t key) {
    if (!tree || !tree->root) return VNULL;
    btree_val_t value = btree_search_node(tree->root, key, tree->cmp, tree->t);
    return value;
}

// 更新键对应的值
bool btree_update(btree_t *tree, btree_key_t key, btree_val_t new_value) {
    if (!tree || !tree->root) return false;
    btree_node_t *node = tree->root;
    while (node) {
        int i = 0;
        while (i < node->n && tree->cmp(key, node->keys[i]) > 0)
            i++;
        if (i < node->n && tree->cmp(key, node->keys[i]) == 0) {
            node->values[i] = new_value;
            return true;
        }
        if (node->leaf) break;
        node = node->children[i];
    }
    return false;
}

// 中序遍历（递归）
static void btree_inorder_node(const btree_node_t *node, btree_visit_t visit, void *userdata) {
    if (!node) return;
    for (int i = 0; i < node->n; i++) {
        if (!node->leaf)
            btree_inorder_node(node->children[i], visit, userdata);
        visit(node->keys[i], node->values[i], userdata);
    }
    if (!node->leaf)
        btree_inorder_node(node->children[node->n], visit, userdata);
}

void btree_inorder(const btree_t *tree, btree_visit_t visit, void *userdata) {
    if (!tree || !tree->root) return;
    btree_inorder_node(tree->root, visit, userdata);
}

// 统计节点数量（递归）
static size_t btree_count_node(const btree_node_t *node) {
    if (!node) return 0;
    size_t cnt = node->n;
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++)
            cnt += btree_count_node(node->children[i]);
    }
    return cnt;
}

size_t btree_count(const btree_t *tree) {
    if (!tree || !tree->root) return 0;
    return btree_count_node(tree->root);
}
