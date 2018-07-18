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

a = b = 2
print(a, b)
print(a.__class__.__name__)
