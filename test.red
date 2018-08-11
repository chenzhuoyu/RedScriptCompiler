#!/usr/bin/env redscript

native "C" class test(ldflags = '-L/usr/local/lib -lev')
{
extern void *ev_loop_new(unsigned int flags);

enum foo_t {
    A,
    B = 555,
    C,
};
}

print(dir(test))
print(dir(test.foo_t))
print(test.foo_t.A)
print(test.foo_t.B)
print(test.foo_t.C)
