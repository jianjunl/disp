#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef BTREE_DEFAULT
#define BTREE_DEFAULT 1
#endif

#if BTREE_DEFAULT

typedef uint64_t btree_key_t;
typedef void*    btree_val_t;

#define BT_MALLOC(n) malloc(n)
#define BT_FREE(v)   free(v)
#define VNULL NULL
#define KNULL 0

#else // BTREE_DEFAULT

#include "../btdef.h"

#endif // BTREE_DEFAULT

// B树节点结构（前向声明）
typedef struct btree_node btree_node_t;

// 比较函数：返回 -1 表示 a < b, 0 表示相等, 1 表示 a > b
typedef int (*btree_cmp_t)(btree_key_t a, btree_key_t b);

// B树主结构
typedef struct {
    btree_node_t *root;         // 根节点
    btree_cmp_t cmp;            // 比较函数
    int t;                      // 最小度数（每个节点至少有 t-1 个键）
} btree_t;

// 创建B树：t 为最小度数（t >= 2），cmp 为比较函数（若为 NULL 则使用默认数值比较）
btree_t* btree_create(int t, btree_cmp_t cmp);

// 销毁B树（释放所有节点内存）
void btree_destroy(btree_t *tree);

// 插入键值对
void btree_insert(btree_t *tree, btree_key_t key, btree_val_t value);

// 删除键（返回值：是否删除成功）
bool btree_delete(btree_t *tree, btree_key_t key);

// 查找键：返回值指针（NULL 表示未找到）
btree_val_t btree_search(const btree_t *tree, btree_key_t key);

// 更新键对应的值（如果键存在）
bool btree_update(btree_t *tree, btree_key_t key, btree_val_t new_value);

// 中序遍历（回调函数，参数为 key, value, userdata）
typedef void (*btree_visit_t)(btree_key_t key, btree_val_t value, void *userdata);
void btree_inorder(const btree_t *tree, btree_visit_t visit, void *userdata);

// 获取节点数量（可选）
size_t btree_count(const btree_t *tree);

#endif // BTREE_H
