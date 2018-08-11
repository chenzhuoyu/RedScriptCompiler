#!/usr/bin/env redscript

native "C" class test()
{
    int printf(const char *fmt, ...);
    long strlen(const char *s);

    int foo(int *x);
    long bar(const char **xp);

    int foo(int *x) {
        printf("x is %p\n", x);
        printf("*x is %d\n", *x);
        *x += 1;
        printf("now *x is %d\n", *x);
        return *x * *x;
    }

    long bar(const char **xp) {
        printf("xp is %p\n", xp);
        printf("*xp is %p\n", *xp);
        *xp = "hello, world";
        printf("now *xp is %p\n", *xp);
        return strlen(*xp);
    }
}

import ffi
x = ffi.int32_t(123)
print(test.foo)
print(x)
print(ffi.ref(x))
print(test.foo(ffi.ref(x)))
print(x)
print('***********')
a = ffi.char_p()
print(a)
n = test.bar(ffi.ref(a))
print(n)
print(a)
print('***********')
print(repr(ffi.string_at(a, n)))
