
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

int main(void) {
    printf("sizeof(char) = %ld\n", sizeof(char));
    printf("sizeof(short) = %ld\n", sizeof(short));
    printf("sizeof(int) = %ld\n", sizeof(int));
    printf("sizeof(long) = %ld\n", sizeof(long));
    printf("sizeof(float) = %ld\n", sizeof(float));
    printf("sizeof(double) = %ld\n", sizeof(double));
    printf("sizeof(uint8_t) = %ld\n", sizeof(uint8_t));
    printf("sizeof(uint16_t) = %ld\n", sizeof(uint16_t));
    printf("sizeof(uint32_t) = %ld\n", sizeof(uint32_t));
    printf("sizeof(uint64_t) = %ld\n", sizeof(uint64_t));
    printf("max(long) = %ld\n", (long)0x7FFFFFFFFFFFFFFF);
    printf("min(long) = %ld\n", (long)0x8000000000000000);
    printf("max(long) = %lX\n", (long)0x7FFFFFFFFFFFFFFF);
    printf("min(long) = %lX\n", (long)0x8000000000000000);
    printf("max(long) = %ld\n", LONG_MAX);
    printf("max(long) = %lX\n", LONG_MAX);
    printf("min(long) = %ld\n", LONG_MIN);
    printf("min(long) = %lX\n", LONG_MIN);
    printf("min(long) = %ld\n", (-LONG_MAX - 1L));
    printf("M(long) = %lX\n", (long)0x0000FFFFFFFFFFFF);
    printf("N(long) = %lX\n", (long)0x8FFF000000000000);
    printf("M(long) = %ld\n", (long)0x0000FFFFFFFFFFFF);
    printf("N(long) = %ld\n", (long)0x8FFF000000000000);
}
