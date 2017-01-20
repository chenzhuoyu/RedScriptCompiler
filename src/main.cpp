#include <stdio.h>
#include "Tokenizer.h"

int main()
{
    Tokenizer tk(R"source(
        a.x = 1
    )source");

    while (!tk.peekOrLine()->is<Token::Eof>())
        printf("%s\n", tk.nextOrLine()->toString().c_str());

    return 0;
}