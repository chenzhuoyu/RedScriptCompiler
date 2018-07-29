#!/usr/bin/env redscript
#native 'C' class NativeClass(clfags = '-Wall')
#{
#struct tc_comp_t
#{
#    int val_1;
#    int val_2;
#    struct {
#        int x;
#        int y;
#    };
#};
#
#typedef int (*ff)(long, float);
#int *fun(ff);
#int *fun(ff f) {
#    static int x = 1;
#    return &x;
#}
#
#struct tc_comp_t;
#typedef struct tc_comp_t TestComposite;
#
#static int b = 1000;
#
#long test(TestComposite ts, float f);
#static TestComposite test_func(int arg0, float arg1);
#
#extern int scanf(const char *fmt, ...);
#extern int printf(const char *fmt, ...);
#
#typedef enum {
#    item_1,
#    item_2,
#} test_enum_t;
#
#long test(TestComposite ts, float f)
#{
#    typedef struct tc_comp_t Test123;
#    printf("this is test\n");
#    return test_func(ts.val_1, f).val_2;
#}
#
#static TestComposite test_func(int arg0, float arg1)
#{
#    b += arg0;
#    printf("hello, world from native code, b = %d, &b = %p, this = %p, arg1 = %f\n", b, &b, (void *)test, arg1);
#    TestComposite tc;
#    tc.val_1 = 999;
#    tc.val_2 = 12345;
#    return tc;
#}
#}

# def fac(n) {
#     i = 2
#     r = 1
#     while (i <= n) {
#         r *= i
#         i += 1
#     }
#     return r
# }

# def calc(n) {
#     k = 0
#     t = 0.0
#     pi = 0.0
#     deno = 0.0
#     while (k < n) {
#         t = ((-1) ** k) * (fac(6 * k)) * (13591409.0 + 545140134.0 * k)
#         deno = fac(3 * k) * (fac(k) ** 3) * (640320.0 ** (3 * k))
#         pi += t / deno
#         k += 1
#     }
#     pi = pi * 12 / (640320 ** 1.5)
#     pi = 1.0 / pi
#     return pi
# }

# print(1.0 % 0.3)
# print(calc(350))
class Foo : ValueError {}

def test()
{
    try 
    {
        print('in try')
        return 'hello, world'
    }
    finally
    {
        print('in finally')
    }
}

def foo()
{
    try
    {
        print(test())
    }
    except (Exception as f)
    {
        print('in outer except')
        print(f)
    }
    finally
    {
        raise AttributeError('also nested')
    }
}

try
{
    foo()
}
except (Exception as e)
{
    print('exception traceback below:')
    print()
    print(e.__traceback__())
}