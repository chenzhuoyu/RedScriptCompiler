const char *source = R"source(#!/usr/bin/env redscript
native 'C' class NativeClass()
{
struct tc_comp_t
{
    int val_1;
    int val_2;
    struct {
        int x;
        int y;
    };
};

typedef int (*ff)(long, float);
int *fun(ff);
int *fun(ff f) {
    static int x = 1;
    return &x;
}

struct tc_comp_t;
typedef struct tc_comp_t TestComposite;

static int b = 1000;

long test(TestComposite ts, float f);
static TestComposite test_func(int arg0, float arg1);

extern int scanf(const char *fmt, ...);
extern int printf(const char *fmt, ...);

typedef enum {
    item_1,
    item_2,
} test_enum_t;

long test(TestComposite ts, float f)
{
    typedef struct tc_comp_t Test123;
    printf("this is test\n");
    return test_func(ts.val_1, f).val_2;
}

static TestComposite test_func(int arg0, float arg1)
{
    b += arg0;
    printf("hello, world from native code, b = %d, &b = %p, this = %p, arg1 = %f\n", b, &b, (void *)test, arg1);
    TestComposite tc;
    tc.val_1 = 999;
    tc.val_2 = 12345;
    return tc;
}
}
)source";

#include <iostream>

#include <unistd.h>
#include <libtcc.h>
#include <sys/mman.h>

#include "engine/Memory.h"
#include "engine/GarbageCollector.h"

#include "utils/Strings.h"
#include "runtime/Object.h"
#include "compiler/Parser.h"
#include "compiler/Tokenizer.h"


void run(void)
{
    RedScript::Compiler::Parser parser(std::make_unique<RedScript::Compiler::Tokenizer>(source));
    auto native = parser.parseNative();
    std::cout << "---------------- native code ----------------" << std::endl;
    std::cout << native->code << std::endl;
    std::cout << "---------------------------------------------" << std::endl;

    std::cout << "raw usage: " << RedScript::Engine::Memory::rawUsage() << std::endl;
    std::cout << "array usage: " << RedScript::Engine::Memory::arrayUsage() << std::endl;
    std::cout << "object usage: " << RedScript::Engine::Memory::objectUsage() << std::endl;
    std::cout << "---------------------------------------------" << std::endl;

    TCCState *tcc = tcc_new();
    tcc_set_options(tcc, "-nostdlib");
    tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

    int ret = tcc_compile_string(tcc, native->code.c_str());
    std::cout << "tcc-compile: " << ret << std::endl;

    if (ret < 0)
    {
        tcc_delete(tcc);
        return;
    }

    ret = tcc_relocate(tcc);
    std::cout << "tcc-relocate(): " << ret << std::endl;

    if (ret < 0)
    {
        tcc_delete(tcc);
        return;
    }

    TCCFunction *func = tcc_find_function(tcc, "scanf");
    std::cout << "tcc-find-function(scanf): " << func << std::endl;

    void *fp = tcc_function_get_addr(tcc, func);
    std::cout << "tcc-function-get-addr(scanf): " << fp << std::endl;

    func = tcc_find_function(tcc, "test");
    std::cout << "tcc-find-function(test): " << func << std::endl;

    size_t nargs = tcc_function_get_nargs(func);
    TCCType *rettype = tcc_function_get_return_type(func);
    std::cout << "tcc-function-get-nargs(test): " << nargs << std::endl;
    std::cout << "tcc-function-get-return-type(test): " << rettype << std::endl;

    for (size_t i = 0; i < nargs; i++)
    {
        TCCType *t = tcc_function_get_arg_type(func, i);
        const char *n = tcc_function_get_arg_name(func, i);
        std::cout << "- arg " << i << ": name=" << std::string(n) << ", type=" << t << std::endl;
    }

    fp = tcc_function_get_addr(tcc, func);
    std::cout << "tcc-function-get-addr(test): " << fp << std::endl;

    auto val = ((long (*)(int, float))fp)(555, 1.234);
    std::cout << "native.test(): " << val << std::endl;
    tcc_delete(tcc);
}

int main()
{
    RedScript::Engine::GarbageCollector::initialize(
        1 * 1024 * 1024 * 1024,     /* Young,   1G */
             512 * 1024 * 1024,     /* Old  , 512M */
             128 * 1024 * 1024      /* Perm , 128M */
    );

    run();
    RedScript::Engine::GarbageCollector::shutdown();
    return 0;
}
