#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "btree.h"

// 遍历回调，打印键值
void print_node(bt_key_t key, bt_val_t value, void *userdata) {
    (void)userdata;
    printf("(%llu -> %s) ", (unsigned long long)key, (char*)value);
}

int main() {
    // 创建 B 树，最小度数 t=3，使用默认数值比较
    btree_t *tree = btree_create(NULL);
    assert(tree != NULL);

    // 插入一些键值对
    printf("Inserting keys 1..20\n");
    for (bt_key_t i = 1; i <= 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "val_%llu", (unsigned long long)i);
        // 注意：为了简单，直接传 buf 的副本（实际应动态分配或使用持久存储）
        // 这里用静态分配的临时字符串仅作演示，真实代码中需自行复制
        char *val = malloc(32);
        sprintf(val, "val_%llu", (unsigned long long)i);
        btree_insert(tree, i, (bt_val_t)val);
    }

    // 查找测试
    printf("Searching key 10: ");
    bt_val_t v = btree_search(tree, 10);
    if (v) printf("%s\n", (char*)v);
    else printf("not found\n");

    printf("Searching key 99: ");
    v = btree_search(tree, 99);
    if (v) printf("%s\n", (char*)v);
    else printf("not found\n");

    // 更新测试
    printf("Updating key 10 to 'updated_10'\n");
    char *new_val = malloc(32);
    strcpy(new_val, "updated_10");
    bool ok = btree_update(tree, 10, (bt_val_t)new_val);
    assert(ok);
    v = btree_search(tree, 10);
    printf("After update: %s\n", (char*)v);

    // 删除测试
    printf("Deleting key 5\n");
    ok = btree_delete(tree, 5);
    assert(ok);
    v = btree_search(tree, 5);
    printf("After delete, search 5: %s\n", v ? (char*)v : "NULL");

    // 中序遍历
    printf("Inorder traversal of tree (keys 1..20 except 5):\n");
    btree_inorder(tree, print_node, NULL);
    printf("\n");

    // 统计节点数量
    size_t count = btree_count(tree);
    printf("Total number of key-value pairs: %zu\n", count);

printf("Inserting keys 0xFFFFFFFFFFFFFFF0  ..   0xFFFFFFFFFFFFFFFF\n");
for (int j = 0; j < 16; j++) {
    uint64_t i = 0xFFFFFFFFFFFFFFF0ULL + j;
    btree_insert(tree, i, (bt_val_t)i);
}
printf("Searching keys 0xFFFFFFFFFFFFFFF0  ..   0xFFFFFFFFFFFFFFFF\n");
for (int j = 0; j < 16; j++) {
    uint64_t i = 0xFFFFFFFFFFFFFFF0ULL + j;
    bt_val_t v = btree_search(tree, i);
    if (i == (uint64_t)v) printf("%llx found\n", (unsigned long long)i);
    else printf("%llx not found\n", (unsigned long long)i);
}

    // 清理（需遍历释放每个值的 malloc 内存，但此处为演示简单跳过）
    // 实际应在销毁树之前先遍历 free 所有 value
    btree_destroy(tree);
    printf("Test completed.\n");
    return 0;
}
