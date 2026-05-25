
#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#ifndef DEBUG
//#define DEBUG
#endif
#include "../disp.h"

/* Arithmetic – with type promotion */

#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
static disp_box plus_syscall(disp_box *args, int count) {
    if (count == 0) return disp_make_int(0);

    int has_double = 0, has_float = 0, has_long = 0;
    int max_type = FLAG_INT;

    for (int i = 0; i < count; i++) {
        int t = T(args[i]);
        switch (t) {
            case FLAG_FLOAT: has_float = 1; max_type = FLAG_FLOAT; break;
            case FLAG_DOUBLE: has_double = 1; max_type = FLAG_DOUBLE; break;
            case FLAG_LONG: has_long = 1; if (max_type < FLAG_LONG) max_type = FLAG_LONG; break;
            case FLAG_INT: if (max_type < FLAG_INT) max_type = FLAG_INT; break;
            default: break;
        }
    }

    if (has_double || has_float) {
        long double sum = 0.0L;
        for (int i = 0; i < count; i++) {
            disp_box v = args[i];
            switch (T(v)) {
                case FLAG_INT:    sum += disp_get_int(v); break;
                case FLAG_LONG:   sum += disp_get_long(v); break;
                case FLAG_FLOAT:  sum += disp_get_float(v); break;
                case FLAG_DOUBLE: sum += disp_get_double(v); break;
                default: break;
            }
        }
        if (has_double || max_type == FLAG_DOUBLE)
            return disp_make_double((double)sum);
        else
            return disp_make_float((float)sum);
    } else {
        long long sum = 0;
        for (int i = 0; i < count; i++) {
            disp_box v = args[i];
            switch (T(v)) {
                case FLAG_INT:  sum += disp_get_int(v); break;
                case FLAG_LONG: sum += disp_get_long(v); break;
                default: break;
            }
        }
        if (max_type == FLAG_LONG || sum > INT_MAX || sum < INT_MIN)
            return disp_make_long((long)sum);
        else
            return disp_make_int((int)sum);
    }
}

static disp_box minus_syscall(disp_box *args, int count) {
    if (count == 0) return disp_make_int(0);
    if (count == 1) {
        // unary minus: negate the argument, preserving its type
        disp_box v = args[0];
        int t = T(v);
        switch (t) {
            case FLAG_INT:    return disp_make_int(-disp_get_int(v));
            case FLAG_LONG:   return disp_make_long(-disp_get_long(v));
            case FLAG_FLOAT:  return disp_make_float(-disp_get_float(v));
            case FLAG_DOUBLE: return disp_make_double(-disp_get_double(v));
            default:
                ERRO("minus: unsupported type\n");
                return NIL;
        }
    }

    // Binary case: result = first - (sum of the rest)
    // We'll compute using type promotion similar to plus.
    disp_box *rest = args + 1;
    int rest_count = count - 1;

    // First, determine the result type by checking all arguments
    int has_double = 0, has_float = 0, has_long = 0;
    int max_type = FLAG_INT;
    for (int i = 0; i < count; i++) {
        int t = T(args[i]);
        switch (t) {
            case FLAG_FLOAT: has_float = 1; max_type = FLAG_FLOAT; break;
            case FLAG_DOUBLE: has_double = 1; max_type = FLAG_DOUBLE; break;
            case FLAG_LONG: has_long = 1; if (max_type < FLAG_LONG) max_type = FLAG_LONG; break;
            case FLAG_INT: if (max_type < FLAG_INT) max_type = FLAG_INT; break;
            default: break;
        }
    }

    if (has_double || has_float) {
        long double result = 0.0L;
        // first argument
        switch (T(args[0])) {
            case FLAG_INT:    result = disp_get_int(args[0]); break;
            case FLAG_LONG:   result = disp_get_long(args[0]); break;
            case FLAG_FLOAT:  result = disp_get_float(args[0]); break;
            case FLAG_DOUBLE: result = disp_get_double(args[0]); break;
            default: result = 0.0L;
        }
        // subtract the rest
        for (int i = 0; i < rest_count; i++) {
            disp_box v = rest[i];
            switch (T(v)) {
                case FLAG_INT:    result -= disp_get_int(v); break;
                case FLAG_LONG:   result -= disp_get_long(v); break;
                case FLAG_FLOAT:  result -= disp_get_float(v); break;
                case FLAG_DOUBLE: result -= disp_get_double(v); break;
                default: break;
            }
        }
        if (has_double || max_type == FLAG_DOUBLE)
            return disp_make_double((double)result);
        else
            return disp_make_float((float)result);
    } else {
        long long result = 0;
        // first argument
        switch (T(args[0])) {
            case FLAG_INT:  result = disp_get_int(args[0]); break;
            case FLAG_LONG: result = disp_get_long(args[0]); break;
            default: result = 0;
        }
        // subtract the rest
        for (int i = 0; i < rest_count; i++) {
            disp_box v = rest[i];
            switch (T(v)) {
                case FLAG_INT:  result -= disp_get_int(v); break;
                case FLAG_LONG: result -= disp_get_long(v); break;
                default: break;
            }
        }
        if (max_type == FLAG_LONG || result > INT_MAX || result < INT_MIN)
            return disp_make_long((long)result);
        else
            return disp_make_int((int)result);
    }
}

static disp_box times_syscall(disp_box *args, int count) {
    if (count == 0) return disp_make_int(1);

    int has_double = 0, has_float = 0, has_long = 0;
    int max_type = FLAG_INT;

    for (int i = 0; i < count; i++) {
        int t = T(args[i]);
        switch (t) {
            case FLAG_FLOAT: has_float = 1; max_type = FLAG_FLOAT; break;
            case FLAG_DOUBLE: has_double = 1; max_type = FLAG_DOUBLE; break;
            case FLAG_LONG: has_long = 1; if (max_type < FLAG_LONG) max_type = FLAG_LONG; break;
            case FLAG_INT: if (max_type < FLAG_INT) max_type = FLAG_INT; break;
            default: break;
        }
    }

    if (has_double || has_float) {
        long double prod = 1.0L;
        for (int i = 0; i < count; i++) {
            disp_box v = args[i];
            switch (T(v)) {
                case FLAG_INT:    prod *= disp_get_int(v); break;
                case FLAG_LONG:   prod *= disp_get_long(v); break;
                case FLAG_FLOAT:  prod *= disp_get_float(v); break;
                case FLAG_DOUBLE: prod *= disp_get_double(v); break;
                default: break;
            }
        }
        if (has_double || max_type == FLAG_DOUBLE)
            return disp_make_double((double)prod);
        else
            return disp_make_float((float)prod);
    } else {
        long long prod = 1;
        for (int i = 0; i < count; i++) {
            disp_box v = args[i];
            switch (T(v)) {
                case FLAG_INT:  prod *= disp_get_int(v); break;
                case FLAG_LONG: prod *= disp_get_long(v); break;
                default: break;
            }
        }
        if (max_type == FLAG_LONG || prod > INT_MAX || prod < INT_MIN)
            return disp_make_long((long)prod);
        else
            return disp_make_int((int)prod);
    }
}

static disp_box divide_syscall(disp_box *args, int count) {
    // Division always returns double for simplicity.
    // Special cases: empty or single argument.
    if (count == 0) return disp_make_double(0.0);
    if (count == 1) {
        double d = 1.0;
        disp_box v = args[0];
        switch (T(v)) {
            case FLAG_INT:    d /= disp_get_int(v); break;
            case FLAG_LONG:   d /= disp_get_long(v); break;
            case FLAG_FLOAT:  d /= disp_get_float(v); break;
            case FLAG_DOUBLE: d /= disp_get_double(v); break;
            default: d = 0.0;
        }
        return disp_make_double(d);
    }

    // First argument is numerator, rest are divisors.
    double result = 0.0;
    disp_box v0 = args[0];
    switch (T(v0)) {
        case FLAG_INT:    result = disp_get_int(v0); break;
        case FLAG_LONG:   result = disp_get_long(v0); break;
        case FLAG_FLOAT:  result = disp_get_float(v0); break;
        case FLAG_DOUBLE: result = disp_get_double(v0); break;
        default: result = 0.0;
    }
    for (int i = 1; i < count; i++) {
        disp_box v = args[i];
        switch (T(v)) {
            case FLAG_INT:    result /= disp_get_int(v); break;
            case FLAG_LONG:   result /= disp_get_long(v); break;
            case FLAG_FLOAT:  result /= disp_get_float(v); break;
            case FLAG_DOUBLE: result /= disp_get_double(v); break;
            default: break;
        }
    }
    return disp_make_double(result);
}

/* Initialisation function called when the shared library is loaded */
void disp_init_module(void) {
    DEF("+", MKF(plus_syscall  , "<plus>"  ), 1);
    DEF("-", MKF(minus_syscall , "<minus>" ), 1);
    DEF("*", MKF(times_syscall , "<times>" ), 1);
    DEF("/", MKF(divide_syscall, "<divide>"), 1);
}
