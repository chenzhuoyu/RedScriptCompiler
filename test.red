#!/usr/bin/env redscript

native 'C' class NativeClass()
{
extern int add(int x, int y);
long printf(const char *fmt, ...);

int add(int x, int y) {
    printf("x is %d, y is %d\n", x, y);
    return x + y;
}
}

print(dir(NativeClass))
print(NativeClass.add)
print(NativeClass.add(x = 1, y = 2))
