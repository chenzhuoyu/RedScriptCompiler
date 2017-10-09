#ifndef REDSCRIPT_COMPILER_PARSER_H
#define REDSCRIPT_COMPILER_PARSER_H

#include <memory>
#include <unordered_map>

#include "compiler/Tokenizer.h"

namespace RedScript::Compiler
{
class Parser
{
    int _loops;
    int _functions;
    std::unique_ptr<Tokenizer> _tk;

};
}

#endif /* REDSCRIPT_COMPILER_PARSER_H */
