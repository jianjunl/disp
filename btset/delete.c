#include "bt_private.h"
#include <stdlib.h>

// 在子树中查找最小键（用于后继）
static bt_key_t get_min_key(bt_node_t *node) {
    while (!node->leaf)
        node = node->children[0];
    return node->keys[0];
}

// 在子树中查找最大键（用于前驱）
static bt_key_t get_max_key(bt_node_t *node) {
    while (!node->leaf)
        node = node->children[node->n];
    return node->keys[node->n - 1];
}

// 从节点中删除指定键（内部递归）
void btree_delete_node(btree_t *tree, bt_node_t *node, bt_key_t key) {
    int t = tree->conf->tier;
    int i = 0;
    // 找到第一个 >= key 的位置
    while (i < node->n && tree->conf->cmp(key, node->keys[i]) > 0)
        i++;

    if (i < node->n && tree->conf->cmp(key, node->keys[i]) == 0) {
        // 键在当前节点中
        if (node->leaf) {
            // 叶子节点：直接删除
            for (int j = i; j < node->n - 1; j++) {
                node->keys[j] = node->keys[j + 1];
            }
            node->n--;
            tree->size--;   // <-- 添加这一行
        } else {
            // 内部节点：用前驱或后继替换
            bt_node_t *left_child = node->children[i];
            bt_node_t *right_child = node->children[i + 1];

            if (left_child->n >= t) {
                // 前驱：左子树的最大键
                bt_key_t pred_key = get_max_key(left_child);
                node->keys[i] = pred_key;
                btree_delete_node(tree, left_child, pred_key);
            } else if (right_child->n >= t) {
                // 后继：右子树的最小键
                bt_key_t succ_key = get_min_key(right_child);
                node->keys[i] = succ_key;
                btree_delete_node(tree, right_child, succ_key);
            } else {
                // 两个孩子都不足 t，合并它们
                btree_merge_children(tree, node, i);
                btree_delete_node(tree, left_child, key);
            }
        }
    } else {
        // 键不在当前节点，应该在子节点中
        if (node->leaf) {
            // 未找到，直接返回
            return;
        }
        bt_node_t *child = node->children[i];
        // 如果子节点键数不足，需要先补充
        if (child->n == t - 1) {
            bt_node_t *left_sib = (i > 0) ? node->children[i - 1] : NULL;
            bt_node_t *right_sib = (i < node->n) ? node->children[i + 1] : NULL;

            if (left_sib && left_sib->n >= t) {
                btree_borrow_from_left(tree, node, i);
            } else if (right_sib && right_sib->n >= t) {
                btree_borrow_from_right(tree, node, i);
            } else {
                // 与左兄弟或右兄弟合并
                if (left_sib) {
                    btree_merge_children(tree, node, i - 1);
                    child = node->children[i - 1];
                } else if (right_sib) {
                    btree_merge_children(tree, node, i);
                    child = node->children[i];
                }
            }
        }
        btree_delete_node(tree, child, key);
    }
}

// 公共删除接口
bool btree_delete(btree_t *tree, bt_key_t key) {
    if (!tree || !tree->root) return false;
    btree_delete_node(tree, tree->root, key);
    // 如果根节点变空，则将第一个孩子作为新根（如果存在）
    if (tree->root->n == 0) {
        bt_node_t *old_root = tree->root;
        if (!old_root->leaf) {
            tree->root = old_root->children[0];
        } else {
            tree->root = NULL;
        }
        tree->conf->free(old_root->keys);
        tree->conf->free(old_root->children);
        tree->conf->free(old_root);
    }
    return true;
}
