#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h> // For ssize_t

int main() {
printf("sizeof(int*) = %ld\n", sizeof(int*));
printf("sizeof(long*) = %ld\n", sizeof(long*));
printf("sizeof(int) = %ld\n", sizeof(int));
printf("sizeof(long) = %ld\n", sizeof(long));
    return 0;
}
