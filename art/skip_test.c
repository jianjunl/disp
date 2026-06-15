
// gcc -g -std=c11 -fPIC -Wall -Wextra -o skip_test skip_test.c skip.c

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "skip.h"

void test_number() {
    skip_list *sl = skip_create(NULL);
    skip_insert(sl, 3);
    skip_insert(sl, 6);
    skip_insert(sl, 0xFFFFFFFFFFFFFFFF);
    skip_insert(sl, 0xFFFFFFFFFFFFFFFc);
    skip_insert(sl, 0x0FFFFFFFFFFFFFFc);
    skip_insert(sl, 0xAFFFFFFFFFFFFFFc);
    skip_insert(sl, 7);
    skip_insert(sl, 9);
    skip_insert(sl, 12);
    skip_insert(sl, 19);
    skip_insert(sl, 17);
    skip_insert(sl, 26);
    skip_insert(sl, 21);
    skip_insert(sl, 25);

    printf("Search 3: %s\n", skip_search(sl, 3) ? "true" : "false");
    printf("Search 19: %s\n", skip_search(sl, 19) ? "true" : "false");
    printf("Search 0xFFFFFFFFFFFFFFFF: %s\n", skip_search(sl, 0xFFFFFFFFFFFFFFFF) ? "true" : "false");
    printf("Search 0xFFFFFFFFFFFFFFFC: %s\n", skip_search(sl, 0xFFFFFFFFFFFFFFFC) ? "true" : "false");
    printf("Search 0x0FFFFFFFFFFFFFFC: %s\n", skip_search(sl, 0x0FFFFFFFFFFFFFFC) ? "true" : "false");
    printf("Search 0xAFFFFFFFFFFFFFFC: %s\n", skip_search(sl, 0xAFFFFFFFFFFFFFFC) ? "true" : "false");
    printf("Search 0xBFFFFFFFFFFFFFFC: %s\n", skip_search(sl, 0xBFFFFFFFFFFFFFFC) ? "true" : "false");
    printf("Search 20: %s\n", skip_search(sl, 20) ? "true" : "false");

    skip_delete(sl, 19);
    printf("Search 19 after deletion: %s\n", skip_search(sl, 19) ? "true" : "false");

    skip_destroy(sl);
}

static inline int string_cmp(uintptr_t a, uintptr_t b) {
    return strcmp((char*)a, (char*)b);
}

static skip_conf conf_string = (skip_conf) {
    .cmp    = string_cmp,
    .malloc = malloc,
    .calloc = calloc,
    .free   = free
}; 

static char* s1 = "aaaaa";
static char* s2 = "baaaa";
static char* s3 = "acaaa";
static char* s4 = "aaada";
static char* s5 = "dddaaaaa";

void test_string() {
    skip_list *sl = skip_create(&conf_string);
    skip_insert(sl, (uintptr_t)s1);
    skip_insert(sl, (uintptr_t)s2);
    skip_insert(sl, (uintptr_t)s3);
    skip_insert(sl, (uintptr_t)s4);
    skip_insert(sl, (uintptr_t)s5);

    printf("Search %s: %s\n", s1, skip_search(sl, (uintptr_t)s1) ? "true" : "false");
    printf("Search %s: %s\n", s2, skip_search(sl, (uintptr_t)s2) ? "true" : "false");
    printf("Search %s: %s\n", s3, skip_search(sl, (uintptr_t)s3) ? "true" : "false");
    printf("Search %s: %s\n", s4, skip_search(sl, (uintptr_t)s4) ? "true" : "false");
    printf("Search %s: %s\n", s5, skip_search(sl, (uintptr_t)s5) ? "true" : "false");

    skip_delete(sl, (uintptr_t)s4);
    printf("Search %s: %s\n", s4, skip_search(sl, (uintptr_t)s4) ? "true" : "false");

    skip_destroy(sl);
}

int main() {
    test_number();
    test_string();
    return 0;
}
