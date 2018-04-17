const char *source = R"source(#!/usr/bin/env redscript
native 'C' class NativeClass()
{
int printf(const char *fmt, ...);

static int b;
int test(int a)
{
    a += 100;
    b = a;
    printf("hello, world from native code, b = %d\n", b);
    return 12345;
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

    int size = tcc_relocate(tcc, nullptr);
    std::cout << "tcc-relocate(NULL): " << size << std::endl;

    if (size < 0)
    {
        tcc_delete(tcc);
        return;
    }

    long ps = sysconf(_SC_PAGESIZE);
    auto len = (static_cast<size_t>(size) / ps + 1) * ps;
    void *mem = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    std::cout << "mmap(" << len << "): " << mem << std::endl;

    if (mem == MAP_FAILED)
    {
        tcc_delete(tcc);
        return;
    }

    ret = tcc_relocate(tcc, mem);
    std::cout << "tcc-relocate(" << mem << "): " << ret << std::endl;

    ret = mprotect(mem, len, PROT_READ | PROT_EXEC);
    std::cout << "mprotect(" << mem << ", 'r-x'): " << ret << std::endl;

    void *func = tcc_get_symbol(tcc, "test");
    std::cout << "tcc-get-symbol(test): " << func << std::endl;

    ret = ((int (*)(int))func)(555);
    std::cout << "native.test(): " << ret << std::endl;
    tcc_delete(tcc);
    munmap(mem, len);
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
