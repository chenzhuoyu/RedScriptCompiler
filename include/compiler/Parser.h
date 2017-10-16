#ifndef REDSCRIPT_COMPILER_PARSER_H
#define REDSCRIPT_COMPILER_PARSER_H

#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "compiler/AST.h"
#include "compiler/Tokenizer.h"

#include "utils/Strings.h"
#include "runtime/Object.h"

namespace RedScript::Compiler
{
class Parser final
{
    int _loops;
    int _functions;
    std::unique_ptr<Tokenizer> _lexer;
    std::unordered_map<std::string, Runtime::ObjectRef> _builtins;

private:
    class Scope
    {
        int &_value;

    public:
       ~Scope() { _value--; }
        Scope(int &value) : _value(value) { _value++; }

    };

private:
    class Reset
    {
        int _old;
        int &_value;

    public:
       ~Reset() { _value = _old; }
        Reset(int &value) : _old(value), _value(value) { value = 0; }

    };

public:
    Parser(std::unique_ptr<Tokenizer> &&lexer) : _lexer(std::move(lexer)), _loops(0), _functions(0) {}

public:
    std::unique_ptr<AST::Node> parse(void);

public:
    void addBuiltin(const std::string &name, Runtime::ObjectRef object)
    {
        if (_builtins.find(name) == _builtins.end())
            _builtins.insert({ name, object });
        else
            throw std::invalid_argument(Utils::Strings::format("Object named \"%s\" already exists", name));
    }

/*** Basic Language Structures ***/

public:
    std::unique_ptr<AST::Function>      parseLambda(void);

/*** Control Flows ***/

public:
    std::unique_ptr<AST::Break>         parseBreak(void);
    std::unique_ptr<AST::Return>        parseReturn(void);
    std::unique_ptr<AST::Continue>      parseContinue(void);

/*** Object Modifiers ***/

public:
    std::unique_ptr<AST::Index>         parseIndex(void);
    std::unique_ptr<AST::Invoke>        parseInvoke(void);
    std::unique_ptr<AST::Attribute>     parseAttribute(void);

/*** Expressions ***/

public:
    enum class CompositeSuggestion : int
    {
        Tuple,
        Lambda,
        Simple,
    };

public:
    std::unique_ptr<AST::Name>              parseName(void);
    std::unique_ptr<AST::Unpack>            parseUnpack(void);
    std::unique_ptr<AST::Literal>           parseLiteral(void);
    std::unique_ptr<AST::Composite>         parseComposite(CompositeSuggestion suggestion);
    std::unique_ptr<AST::Expression>        parseExpression(void);
    std::unique_ptr<AST::Expression>        parseReturnExpression(void);

private:
    bool isName(const std::unique_ptr<AST::Composite> &comp);
    bool isName(const std::unique_ptr<AST::Expression> &expr);
    void pruneExpression(std::unique_ptr<AST::Expression> &expr);

public:
    std::unique_ptr<AST::Expression>        parseContains(void);
    std::unique_ptr<AST::Expression>        parseBoolOr(void);
    std::unique_ptr<AST::Expression>        parseBoolAnd(void);
    std::unique_ptr<AST::Expression>        parseBitOr(void);
    std::unique_ptr<AST::Expression>        parseBitXor(void);
    std::unique_ptr<AST::Expression>        parseBitAnd(void);
    std::unique_ptr<AST::Expression>        parseEquals(void);
    std::unique_ptr<AST::Expression>        parseCompares(void);
    std::unique_ptr<AST::Expression>        parseShifts(void);
    std::unique_ptr<AST::Expression>        parseAddSub(void);
    std::unique_ptr<AST::Expression>        parseTerm(void);
    std::unique_ptr<AST::Expression>        parsePower(void);
    std::unique_ptr<AST::Expression>        parseFactor(void);

/*** Generic Statements ***/

public:
    std::unique_ptr<AST::Statement>         parseStatement(void);
    std::unique_ptr<AST::CompondStatement>  parseCompondStatement(void);

};
}

#endif /* REDSCRIPT_COMPILER_PARSER_H */
