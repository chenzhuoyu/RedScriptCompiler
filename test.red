#!/usr/bin/env redscript

native 'C' class NativeClass()
{
extern int add(int *x, int *y);

int add(int *x, int *y) {
    return *x + *y;
}
}

print(dir(NativeClass))
print(NativeClass.add);
