#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// B树节点结构（前向声明）
typedef struct btree_node btree_node_t;

// 比较函数：返回 -1 表示 a < b, 0 表示相等, 1 表示 a > b
typedef int (*btree_cmp_t)(uint64_t a, uint64_t b);

// B树主结构
typedef struct {
    btree_node_t *root;         // 根节点
    int t;                      // 最小度数（每个节点至少有 t-1 个键）
    btree_cmp_t cmp;            // 比较函数
} btree_t;

// 创建B树：t 为最小度数（t >= 2），cmp 为比较函数（若为 NULL 则使用默认数值比较）
btree_t* btree_create(int t, btree_cmp_t cmp);

// 销毁B树（释放所有节点内存）
void btree_destroy(btree_t *tree);

// 插入键值对
void btree_insert(btree_t *tree, uint64_t key, void *value);

// 删除键（返回值：是否删除成功）
bool btree_delete(btree_t *tree, uint64_t key);

// 查找键：返回值指针（NULL 表示未找到）
void* btree_search(const btree_t *tree, uint64_t key);

// 更新键对应的值（如果键存在）
bool btree_update(btree_t *tree, uint64_t key, void *new_value);

// 中序遍历（回调函数，参数为 key, value, userdata）
typedef void (*btree_visit_t)(uint64_t key, void *value, void *userdata);
void btree_inorder(const btree_t *tree, btree_visit_t visit, void *userdata);

// 获取节点数量（可选）
size_t btree_count(const btree_t *tree);

// GC 版本创建（树节点由 GC 管理，值存储不受 GC 追踪，但可额外保护）
btree_t* btree_create_gc(int t, btree_cmp_t cmp);

#endif // BTREE_H
