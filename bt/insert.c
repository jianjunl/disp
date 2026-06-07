#include "bt_private.h"
#include <stdlib.h>

// 在非满节点中插入键值对（辅助函数）
void btree_insert_nonfull(btree_t *tree, bt_node_t *node, bt_key_t key, bt_val_t value) {
    int t = tree->t;
    int i = node->n - 1;
    
    if (node->leaf) {
        // 叶子节点：找到插入位置并移动后续元素
        while (i >= 0 && tree->cmp(key, node->keys[i]) < 0) {
            node->keys[i+1] = node->keys[i];
            node->values[i+1] = node->values[i];
            i--;
        }
        node->keys[i+1] = key;
        node->values[i+1] = value;
        node->n++;
    } else {
        // 内部节点：找到合适的子节点
        while (i >= 0 && tree->cmp(key, node->keys[i]) < 0)
            i--;
        i++;
        // 如果子节点已满，先分裂
        if (node->children[i]->n == 2*t - 1) {
            btree_split_child(tree, node, i);
            // 分裂后，决定走左子还是右子
            if (tree->cmp(key, node->keys[i]) > 0)
                i++;
        }
        btree_insert_nonfull(tree, node->children[i], key, value);
    }
}

// 插入主函数
void btree_insert(btree_t *tree, bt_key_t key, bt_val_t value) {
    if (!tree || !tree->root) return;
    
    bt_node_t *root = tree->root;
    int t = tree->t;
    
    // 如果根节点已满，需要分裂并创建新根
    if (root->n == 2*t - 1) {
        bt_node_t *new_root = bt_node_create(t, false);
        if (!new_root) return;
        new_root->children[0] = root;
        tree->root = new_root;
        btree_split_child(tree, new_root, 0);
        // 新根分裂后，现在两个子节点，确定插入哪个子节点
        int i = 0;
        if (tree->cmp(key, new_root->keys[0]) > 0)
            i++;
        btree_insert_nonfull(tree, new_root->children[i], key, value);
    } else {
        btree_insert_nonfull(tree, root, key, value);
    }
}
