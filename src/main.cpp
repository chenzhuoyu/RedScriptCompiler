#include <stdio.h>
#include "compiler/Tokenizer.h"

int main()
{
    RedScript::Compiler::Tokenizer tk(R"source(
        a.x = 1
    )source");

    while (!tk.peekOrLine()->is<RedScript::Compiler::Token::Eof>())
        printf("%s\n", tk.nextOrLine()->toString().c_str());

    return 0;
}