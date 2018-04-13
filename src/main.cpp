const char *source = R"source(#!/usr/bin/env redscript

# class Foo : Bar
# {
    def __init__(self, a, b = 10, *c, **d);

    def func(self, x, y)
    {
        if (x > y)
            println('hello, world %d' % (self.a + self.b + 1))

        for (i in range(1, 10))
        {
            self.a += x
            println(self.b + y)
        }
    }
# }

)source";

#include <iostream>

#include "engine/Memory.h"
#include "engine/GarbageCollector.h"

#include "utils/Strings.h"
#include "runtime/Object.h"
#include "compiler/Parser.h"
#include "compiler/Tokenizer.h"


void run(void)
{
    RedScript::Compiler::Parser parser(std::make_unique<RedScript::Compiler::Tokenizer>(source));
    try
    {
        parser.parse();
    } catch (const RedScript::Runtime::SyntaxError &e)
    {
        fprintf(stderr, "%s at %d:%d\n", e.message().c_str(), e.row(), e.col());
        throw;
    }
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
