const char *source = R"source(#!/usr/bin/env redscript
native 'C' class NativeClass()
{
struct tc_comp_t;
typedef struct tc_comp_t TestComposite;

static int b = 1000;

long test(TestComposite ts);
static TestComposite test_func(int arg0);
extern int printf(const char *fmt, ...);

struct tc_comp_t
{
    int val_1;
    int val_2;
};

long test(TestComposite ts)
{
    typedef struct tc_comp_t Test123;
    printf("this is test\n");
    return test_func((int)(Test123)ts).val_1;
}

static TestComposite test_func(int arg0)
{
    b += ((TestComposite)arg0).val_1;
    const char *fmt = "hello, world from native code, b = %d, &b = %p, this = %p, fmt = %p\n";
    printf(fmt, b, &b, (void *)test, (void *)fmt);
    TestComposite tc;
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

    void *func = tcc_get_symbol(tcc, "test");
    std::cout << "tcc-get-symbol(test): " << func << std::endl;

    auto val = ((long (*)(int))func)(555);
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
