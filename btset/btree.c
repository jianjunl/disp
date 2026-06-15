
#include "btree.h"
#include "bt_private.h"   // 确保声明一致
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 默认比较函数（数值比较）
static inline int default_cmp(uint64_t a, uint64_t b) {
    return a < b ? -1 : a > b ? 1 : 0;
}

static bt_conf_t default_conf = (struct bt_conf) {
    .malloc = malloc,
    .calloc = calloc,
    .free   = free,
    .cmp    = default_cmp,
    .tier   = 3
}; 

void bt_node_destroy(btree_t *tree, bt_node_t *node, int t) {
    if (!node) return;
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            bt_node_destroy(tree, node->children[i], t);
        }
    }
    tree->conf->free(node->keys);
    tree->conf->free(node->children);
    tree->conf->free(node);
}

// 销毁B树
void btree_destroy(btree_t *tree) {
    if (!tree) return;
    if (tree->root)
        bt_node_destroy(tree, tree->root, tree->conf->tier);
    void (*f)(void *ptr) = tree->conf->free;
    tree->conf->free(tree->conf);
    f(tree);
}

// 创建B树
btree_t* btree_create(bt_conf_t *conf) {
    bt_conf_t *c;
    if (!conf) c = &default_conf;
    else {
        c = conf->malloc(sizeof(bt_conf_t));
        memcpy(c, conf, sizeof(bt_conf_t));
    }
    if (c->tier < 2) c->tier = 2;   // 最小度数至少为2
    if (!c->malloc) c->malloc = malloc;
    if (!c->calloc) c->calloc = calloc;
    if (!c->free) c->free = free;
    if (!c->cmp) c->cmp = default_cmp;
    btree_t *tree = (btree_t*)c->malloc(sizeof(btree_t));
    if (!tree) return NULL;
    tree->conf = c;
    tree->size = 0;
    // 创建根节点（初始为叶子）
    tree->root = bt_node_create(tree, true);
    if (!tree->root) {
        tree->conf->free(tree);
        return NULL;
    }
    return tree;
}

// 创建新节点（非静态，供其他模块使用）
bt_node_t* bt_node_create(btree_t *tree, bool leaf) {
    bt_node_t *node = (bt_node_t*)tree->conf->malloc(sizeof(bt_node_t));
    if (!node) return NULL;
    int t = tree->conf->tier;
    if (t < 2) t = 2;   // 最小度数至少为2
    node->n = 0;
    node->leaf = leaf;
    node->keys = (bt_key_t*)tree->conf->malloc((2*t - 1) * sizeof(bt_key_t));
    node->children = (bt_node_t**)tree->conf->malloc((2*t) * sizeof(bt_node_t*));
    if (!node->keys || !node->children) {
        tree->conf->free(node->keys); tree->conf->free(node->children); tree->conf->free(node);
        return NULL;
    }
    for (int i = 0; i < 2*t; i++) {
        if (i < 2*t-1) {
            node->keys[i] = KNULL;
        }
        node->children[i] = NULL;
    }
    return node;
}

// 查找键（非静态，供 delete.c 使用）
bool btree_search_node(const bt_node_t *node, bt_key_t key, bt_cmp_t cmp, int t) {
    int i = 0;
    while (i < node->n && cmp(key, node->keys[i]) > 0)
        i++;
    if (i < node->n && cmp(key, node->keys[i]) == 0)
        return true;
    if (node->leaf)
        return false;
    return btree_search_node(node->children[i], key, cmp, t);
}

bool btree_search(const btree_t *tree, bt_key_t key) {
    if (!tree || !tree->root) return false;
    bool ret = btree_search_node(tree->root, key, tree->conf->cmp, tree->conf->tier);
    return ret;
}

// 中序遍历（递归）
static void btree_inorder_node(const bt_node_t *node, btree_visit_t visit, void *userdata) {
    if (!node) return;
    for (int i = 0; i < node->n; i++) {
        if (!node->leaf)
            btree_inorder_node(node->children[i], visit, userdata);
        visit(node->keys[i], userdata);
    }
    if (!node->leaf)
        btree_inorder_node(node->children[node->n], visit, userdata);
}

void btree_inorder(const btree_t *tree, btree_visit_t visit, void *userdata) {
    if (!tree || !tree->root) return;
    btree_inorder_node(tree->root, visit, userdata);
}

// 统计节点数量（递归）
static size_t btree_count_node(const bt_node_t *node) {
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

size_t btree_size(const btree_t *tree) {
    return tree->size;
}
