#include <stdio.h>
#include "engine/Memory.h"
#include "runtime/Object.h"
#include "compiler/Tokenizer.h"

void run(void)
{
    RedScript::Compiler::Tokenizer tk(R"source(#!/usr/bin/env redscript

class Foo : Bar
    def __init__(self, a, b = 10, *c, **d)
    end

    def func(self, x, y)
        if x > y then
            println('hello, world %d' % (self.a + self.b + 1))
        end

        for i in x .. y do
            self.a += x
            println(self.b + y)
        end
    end
end

)source");

    while (!tk.peekOrLine()->is<RedScript::Compiler::Token::Eof>())
        printf("%s\n", tk.nextOrLine()->toString().c_str());

    RedScript::Runtime::ObjectRef obj(new RedScript::Runtime::Object(RedScript::Runtime::MetaType));
}

int main()
{
    run();
    printf("*** rawUsage : %zu\n", RedScript::Engine::Memory::rawUsage());
    printf("*** arrayUsage : %zu\n", RedScript::Engine::Memory::arrayUsage());
    printf("*** objectUsage : %zu\n", RedScript::Engine::Memory::objectUsage());
    return 0;
}
