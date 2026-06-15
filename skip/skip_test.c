
// gcc -g -std=c11 -fPIC -Wall -Wextra -o skip_test skip_test.c skip.c

#include "skip.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

// 打印第 0 层链表（整数）
static void dump_int(skip_list *sl) {
    printf("list: ");
    for (skip_node *n = skip_first(sl); n; n = skip_next(n))
        printf("%ld ", (long)skip_node_value(n));
    printf("\n");
}

// 打印第 0 层链表（字符串）
static void dump_str(skip_list *sl) {
    printf("list: ");
    for (skip_node *n = skip_first(sl); n; n = skip_next(n))
        printf("\"%s\" ", (char *)skip_node_value(n));
    printf("\n");
}

// 验证有序性
static void validate(skip_list *sl, skip_cmp cmp) {
    skip_node *prev = NULL;
    for (skip_node *n = skip_first(sl); n; n = skip_next(n)) {
        if (prev) assert(cmp(skip_node_value(prev), skip_node_value(n)) <= 0);
        prev = n;
    }
}

// ---------- 整数测试 ----------
void test_int() {
    printf("=== 整数测试 ===\n");
    skip_list *sl = skip_create(NULL);
    int vals[] = {3,6,7,9,12,17,19,21,25,26};
    for (size_t i = 0; i < sizeof(vals)/sizeof(vals[0]); i++)
        skip_insert(sl, vals[i]);

    assert(skip_search(sl, 6) == true);
    assert(skip_search(sl, 100) == false);
    assert(skip_delete(sl, 19) == true);
    assert(skip_search(sl, 19) == false);
    assert(skip_delete(sl, 19) == false);

    // 重复插入 12
    skip_insert_dup(sl, 12);
    skip_insert_dup(sl, 12);

    int prev = -1, cnt12 = 0;
    for (skip_node *n = skip_first(sl); n; n = skip_next(n)) {
        int v = (int)skip_node_value(n);
        assert(prev <= v);
        if (v == 12) cnt12++;
        prev = v;
    }
    assert(cnt12 == 3);  // 原有 1 个 + 新插 2 个 = 3 个
    validate(sl, default_cmp);
    skip_destroy(sl);
    printf("整数测试通过\n\n");
}

// ---------- 字符串测试 ----------
static int str_cmp(uintptr_t a, uintptr_t b) {
    return strcmp((char *)a, (char *)b);
}

static skip_conf str_conf = {
    .cmp = str_cmp,
    .malloc = malloc,
    .calloc = calloc,
    .free = free
};

void test_string() {
    printf("=== 字符串测试 ===\n");
    skip_list *sl = skip_create(&str_conf);
    char *s1 = "aaaaa", *s2 = "baaaa", *s3 = "acaaa", *s4 = "aaada", *s5 = "dddaaaaa";
    skip_insert(sl, (uintptr_t)s1);
    skip_insert(sl, (uintptr_t)s2);
    skip_insert(sl, (uintptr_t)s3);
    skip_insert(sl, (uintptr_t)s4);
    skip_insert(sl, (uintptr_t)s5);

    dump_str(sl);
    assert(skip_search(sl, (uintptr_t)s1) == true);
    assert(skip_search(sl, (uintptr_t)s4) == true);
    assert(skip_search(sl, (uintptr_t)"notexist") == false);
    assert(skip_delete(sl, (uintptr_t)s4) == true);
    assert(skip_search(sl, (uintptr_t)s4) == false);

    char *expected[] = {"aaaaa", "acaaa", "baaaa", "dddaaaaa"};
    int idx = 0;
    for (skip_node *n = skip_first(sl); n; n = skip_next(n)) {
        assert(idx < 4);
        assert(strcmp((char *)skip_node_value(n), expected[idx]) == 0);
        idx++;
    }
    validate(sl, str_cmp);
    skip_destroy(sl);
    printf("字符串测试通过\n\n");
}

// ---------- 重复键行为测试 ----------
void test_duplicates() {
    printf("=== 重复键行为测试 ===\n");
    skip_list *sl = skip_create(NULL);
    skip_insert_dup(sl, 5);
    skip_insert_dup(sl, 5);
    skip_insert_dup(sl, 5);
    dump_int(sl);  // 应输出三个 5，顺序与插入相反（后插入的在前）

    int cnt = 0;
    for (skip_node *n = skip_first(sl); n; n = skip_next(n)) cnt++;
    assert(cnt == 3);

    // 删除一个 5（删除第一个节点，即最新插入的）
    assert(skip_delete(sl, 5) == true);
    cnt = 0;
    for (skip_node *n = skip_first(sl); n; n = skip_next(n)) cnt++;
    assert(cnt == 2);

    // 再删除两个
    assert(skip_delete(sl, 5) == true);
    assert(skip_delete(sl, 5) == true);
    assert(skip_first(sl) == NULL);  // 全部删除，链表为空
    assert(skip_delete(sl, 5) == false);
    skip_destroy(sl);
    printf("重复键测试通过\n\n");
}

// ---------- 随机操作及内存泄漏测试（使用唯一键）----------
static size_t alloc_cnt = 0, free_cnt = 0;
static void *test_malloc(size_t sz) { alloc_cnt++; return malloc(sz); }
static void test_free(void *p) { free_cnt++; free(p); }

void test_random_and_memory() {
    printf("=== 随机操作及内存泄漏测试（唯一键）===\n");
    skip_conf mem_conf = {
        .cmp = NULL,
        .malloc = test_malloc,
        .calloc = calloc,
        .free = test_free
    };
    alloc_cnt = free_cnt = 0;
    skip_list *sl = skip_create(&mem_conf);

    const int N = 10000;
    // 生成唯一键（0 ~ N-1）
    int *keys = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) keys[i] = i;

    // 随机打乱顺序插入
    for (int i = N - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = keys[i];
        keys[i] = keys[j];
        keys[j] = tmp;
    }

    for (int i = 0; i < N; i++) skip_insert(sl, keys[i]);
    for (int i = 0; i < N; i++) assert(skip_search(sl, keys[i]) == true);

    // 删除前一半（随机顺序删除）
    for (int i = 0; i < N / 2; i++) {
        assert(skip_delete(sl, keys[i]) == true);
    }
    for (int i = 0; i < N / 2; i++) assert(skip_search(sl, keys[i]) == false);
    for (int i = N / 2; i < N; i++) assert(skip_search(sl, keys[i]) == true);

    // 验证有序性和元素个数
    int prev = -1, cnt = 0;
    for (skip_node *n = skip_first(sl); n; n = skip_next(n)) {
        int v = (int)skip_node_value(n);
        assert(prev < v);   // 严格递增（因为键唯一）
        prev = v;
        cnt++;
    }
    assert(cnt == N - N/2);
    validate(sl, default_cmp);

    skip_destroy(sl);
    free(keys);
    printf("分配次数 = %zu, 释放次数 = %zu\n", alloc_cnt, free_cnt);
    assert(alloc_cnt == free_cnt);
    printf("随机及内存测试通过\n\n");
}

int main() {
    test_int();
    test_string();
    test_duplicates();
    test_random_and_memory();
    printf("========== 所有测试通过 ==========\n");
    return 0;
}
