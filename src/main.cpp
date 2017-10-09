#include <stdio.h>
#include "compiler/Tokenizer.h"

int main()
{
    RedScript::Compiler::Tokenizer tk(R"source(#!/usr/bin/env redscript

class Foo << Bar
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

    return 0;
}
