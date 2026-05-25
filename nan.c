#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

// #include <limits.h>
// #include <string.h>
// #include "./tik.h"

typedef union {
  // sign bits: 1
  // exponent bits: 11
  // quiet/signal bits: 1
  // significand bits: 51
  double as_double;
  uint64_t bits;
  int32_t as_int32;
} value_t;

static const uint64_t max_double = 0xFFF8ULL << 48;
static const uint64_t ptr_mask   = 0xFFF9ULL << 48;
static const uint64_t int_mask   = 0xFFFAULL << 48;

bool is_neg_zero(double x)
{
  return (int)x == 0 && (*(uint64_t*)&x >> 63) == 1;
}

value_t from_double(double x)
{
  value_t v;
  if (x == (int32_t)x && !is_neg_zero(x)) {
    v.bits = (unsigned long) (int32_t)x | int_mask;
    return v;
  }
  v.as_double = x;
  return v;
}

value_t from_ptr(void *p)
{
  uintptr_t p_i = (uintptr_t) p;
  assert((p_i & ptr_mask) == 0);
  return (value_t) { .bits = p_i | ptr_mask };
}

bool is_double(value_t v)
{
  return v.bits <= max_double;
}

bool is_ptr(value_t v)
{
  return (v.bits & ptr_mask) == ptr_mask;
}

bool is_int32(value_t v)
{
  return (v.bits & int_mask) == int_mask;
}

void test(value_t v)
{
  if (is_int32(v)) {
    printf("v is int: %d\n", v.as_int32);
    return;
  }
  if (is_double(v)) {
    printf("v is double: %lf\n", v.as_double);
    return;
  }
  if (is_ptr(v)) {
    printf("v is ptr: %p\n", (void*) (v.bits & ~ptr_mask));
    return;
  }
  puts("IMPOSSIBLE");
  assert(0);
}

int main()
{
  value_t v;
  v = from_double(0.3);
  test(v);
  v = from_double(3);
  test(v);
  v = from_ptr(malloc(1));
  test(v);
  v = from_double(1.0/0);
  test(v);
  v = from_double(sqrt(-1));
  test(v);
  return 0;
}
