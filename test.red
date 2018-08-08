#!/usr/bin/env redscript

native "C" class test()
{
    int printf(const char *fmt, ...);
    int foo(int *x);

    int foo(int *x) {
        printf("x is %p\n", x);
        printf("*x is %d\n", *x);
        *x += 1;
        printf("now *x is %d\n", *x);
        return *x * *x;
    }
}

import ffi
x = ffi.int32_t(123)
print(test.foo)
print(x)
print(ffi.ref(x))
print(test.foo(ffi.ref(x)))
print(x)