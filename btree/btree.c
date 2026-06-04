
#include "btree.h"
#include "btree_private.h"   // 确保声明一致
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../gc/gc.h"

// 默认比较函数（数值比较）
static int default_cmp(uint64_t a, uint64_t b) {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

// 创建新节点（非静态，供其他模块使用）
btree_node_t* btree_node_create(int t, bool leaf) {
    btree_node_t *node = (btree_node_t*)malloc(sizeof(btree_node_t));
    if (!node) return NULL;
    node->n = 0;
    node->leaf = leaf;
    node->keys = (uint64_t*)malloc((2*t - 1) * sizeof(uint64_t));
    node->values = (void**)malloc((2*t - 1) * sizeof(void*));
    node->children = (btree_node_t**)malloc((2*t) * sizeof(btree_node_t*));
    if (!node->keys || !node->values || !node->children) {
        free(node->keys); free(node->values); free(node->children); free(node);
        return NULL;
    }
    for (int i = 0; i < 2*t; i++) {
        if (i < 2*t-1) {
            node->keys[i] = 0;
            node->values[i] = NULL;
        }
        node->children[i] = NULL;
    }
    return node;
}

// 释放节点（仅内部使用，保留 static）
void btree_node_destroy(btree_node_t *node, int t) {
    if (!node) return;
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++) {
            btree_node_destroy(node->children[i], t);
        }
    }
    free(node->keys);
    free(node->values);
    free(node->children);
    free(node);
}

// 查找键（非静态，供 delete.c 使用）
void* btree_search_node(const btree_node_t *node, uint64_t key, btree_cmp_t cmp, int t) {
    int i = 0;
    while (i < node->n && cmp(key, node->keys[i]) > 0)
        i++;
    if (i < node->n && cmp(key, node->keys[i]) == 0)
        return node->values[i];
    if (node->leaf)
        return NULL;
    return btree_search_node(node->children[i], key, cmp, t);
}

// 创建B树
btree_t* btree_create(int t, btree_cmp_t cmp) {
    if (t < 2) t = 2;   // 最小度数至少为2
    btree_t *tree = (btree_t*)malloc(sizeof(btree_t));
    if (!tree) return NULL;
    tree->t = t;
    tree->cmp = cmp ? cmp : default_cmp;
    // 创建根节点（初始为叶子）
    tree->root = btree_node_create(t, true);
    if (!tree->root) {
        free(tree);
        return NULL;
    }
    return tree;
}

// 销毁B树
void btree_destroy(btree_t *tree) {
    if (!tree) return;
    if (tree->root)
        btree_node_destroy(tree->root, tree->t);
    free(tree);
}

void* btree_search(const btree_t *tree, uint64_t key) {
    if (!tree || !tree->root) return NULL;
    return btree_search_node(tree->root, key, tree->cmp, tree->t);
}

// 更新键对应的值
bool btree_update(btree_t *tree, uint64_t key, void *new_value) {
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

static btree_node_t* btree_node_create_gc(int t, bool leaf) {
    btree_node_t *node = (btree_node_t*)gc_typed_malloc(sizeof(btree_node_t), &GC_TYPE_PTR_ARRAY);
    if (!node) return NULL;
    node->n = 0;
    node->leaf = leaf;
    node->keys = (uint64_t*)gc_typed_calloc(2*t - 1, sizeof(uint64_t), &GC_TYPE_PTR_ARRAY);
    node->values = (void**)gc_typed_calloc(2*t - 1, sizeof(void*), &GC_TYPE_PTR_ARRAY);
    node->children = (btree_node_t**)gc_typed_calloc(2*t, sizeof(btree_node_t*), &GC_TYPE_PTR_ARRAY);
    if (!node->keys || !node->values || !node->children) {
        // 注意：GC 版本不需要显式 free，让 GC 回收即可
        return NULL;
    }
    return node;
}

btree_t* btree_create_gc(int t, btree_cmp_t cmp) {
    if (t < 2) t = 2;
    btree_t *tree = (btree_t*)gc_typed_malloc(sizeof(btree_t), &GC_TYPE_PTR_ARRAY);
    if (!tree) return NULL;
    tree->t = t;
    tree->cmp = cmp ? cmp : default_cmp;
    tree->root = btree_node_create_gc(t, true);
    if (!tree->root) return NULL;
    // 将根节点注册为 GC 根（如果整个树需要被扫描）
    //gc_add_root(&tree->root);
    return tree;
}
/*
注意：btree_destroy 对 GC 版本不适用（无需手动 free），可以提供一个 btree_release 或直接让 GC 回收。但为了接口统一，可以保留 btree_destroy，在 GC 版本中空操作或仅移除根。

更合理的做法：不再提供 btree_destroy，而是依赖 GC。但为了向后兼容，可以添加 btree_is_gc 标志。

简化方案：仅在 btree.c 中增加条件分配函数，通过 btree_create_gc 设置内部标志，销毁时根据标志调用不同的释放逻辑。这里暂不展开，用户可根据需要完善。
*/
