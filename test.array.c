/*
gcc -std=c11 -Wall -Wextra -O2 -c array.c -o array.o
gcc -static -std=c11 -Wall -Wextra -O2 test.array.c array.o -o test.array -L gc/. -lgc -lpthread
./test.array
*/
#define _POSIX_C_SOURCE 200809L

#include "array.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// 演示存储 int
void test_int() {
    printf("Test int ...\n");
    disp_array_t *a = disp_array_create(4);
    for (int i = 0; i < 10; ++i)
        disp_array_add(a, (uint64_t)i);
    assert(disp_array_length(a) == 10);
    for (int i = 0; i < 10; ++i)
        assert(disp_array_get(a, i) == (uint64_t)i);
    disp_array_put(a, 5, 99);
    assert(disp_array_get(a, 5) == 99);
    disp_array_destroy(a);
}

// 演示存储 long
void test_long() {
    printf("Test long ...\n");
    disp_array_t *a = disp_array_create(2);
    long vals[] = {100L, 200L, 300L, 123456789012345L};
    for (size_t i = 0; i < sizeof(vals)/sizeof(vals[0]); ++i)
        disp_array_add(a, (uint64_t)vals[i]);
    assert(disp_array_length(a) == 4);
    for (size_t i = 0; i < 4; ++i)
        assert((long)disp_array_get(a, i) == vals[i]);
    disp_array_destroy(a);
}

// 演示存储指针（字符串字面量地址）
void test_pointer() {
    printf("Test pointer ...\n");
    disp_array_t *a = disp_array_create(3);
    const char *s1 = "hello";
    const char *s2 = "lisp";
    const char *s3 = "symbol";
    disp_array_add(a, (uint64_t)(intptr_t)s1);
    disp_array_add(a, (uint64_t)(intptr_t)s2);
    disp_array_add(a, (uint64_t)(intptr_t)s3);
    assert(disp_array_length(a) == 3);
    assert(strcmp((const char*)(intptr_t)disp_array_get(a, 0), s1) == 0);
    assert(strcmp((const char*)(intptr_t)disp_array_get(a, 1), s2) == 0);
    assert(strcmp((const char*)(intptr_t)disp_array_get(a, 2), s3) == 0);
    disp_array_destroy(a);
}

// 边界测试：块大小为1
void test_block1() {
    printf("Test block size = 1 ...\n");
    disp_array_t *a = disp_array_create(1);
    for (int i = 0; i < 100; ++i)
        disp_array_add(a, (uint64_t)i);
    assert(disp_array_length(a) == 100);
    for (int i = 0; i < 100; ++i)
        assert(disp_array_get(a, i) == (uint64_t)i);
    disp_array_destroy(a);
}

int main() {
    test_int();
    test_long();
    test_pointer();
    test_block1();
    printf("All tests passed.\n");
    return 0;
}
