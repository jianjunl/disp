
#define _DEFAULT_SOURCE 
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <endian.h>
#include "art.h"

int main(void) {
    ART t = {0};

    art_insert(&t, (uint8_t*)"abc", 3, (void*)1);
    art_insert(&t, (uint8_t*)"abd", 3, (void*)2);
    art_insert(&t, (uint8_t*)"abe", 3, (void*)3);

    printf("search abd=%ld\n",
           (long)art_search(&t, (uint8_t*)"abd", 3));

    art_delete(&t, (uint8_t*)"abd", 3);

    uint8_t *k; int kl;
    Iter it = art_iter(&t);

    printf("forward:\n");
    while (art_next(&it, &k, &kl))
        printf("%.*s ", kl, k);

    printf("\nreverse:\n");
    it = art_iter(&t);
    while (art_prev(&it, &k, &kl))
        printf("%.*s ", kl, k);

    printf("\n");

    uint64_t keys[] = {
        100,
        200,
        300,
        0xFFFFFFFFFFFFFFFFULL
    };

    uint8_t keybuf[8];

    for (int i = 0; i < 4; i++) {
        u64_to_key(keys[i], keybuf);
        art_insert(&t, keybuf, 8, (void *)(uintptr_t)(keys[i] + 1));
    }

}
