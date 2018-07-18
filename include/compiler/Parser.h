#ifndef REDSCRIPT_COMPILER_PARSER_H
#define REDSCRIPT_COMPILER_PARSER_H

#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "compiler/AST.h"
#include "compiler/Tokenizer.h"

namespace RedScript::Compiler
{
class Parser final
{
    int _cases;
    int _loops;
    int _functions;
    std::unique_ptr<Tokenizer> _lexer;

private:
    class Scope
    {
        int &_value;

    public:
       ~Scope() { _value--; }
        Scope(int &value) : _value(value) { _value++; }

    public:
        template <typename F, typename ... Args>
        static inline auto wrap(int &monitor, F &&func, Args && ... args)
        {
            Scope _(monitor);
            return func(std::forward<Args>(args) ...);
        }
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
    Parser(std::unique_ptr<Tokenizer> &&lexer) :
        _lexer(std::move(lexer)), _cases(0), _loops(0), _functions(0) {}

public:
    std::unique_ptr<AST::CompoundStatement> parse(void);

/*** Basic Language Structures ***/

public:
    std::unique_ptr<AST::If>            parseIf(void);
    std::unique_ptr<AST::For>           parseFor(void);
    std::unique_ptr<AST::Try>           parseTry(void);
    std::unique_ptr<AST::Class>         parseClass(void);
    std::unique_ptr<AST::While>         parseWhile(void);
    std::unique_ptr<AST::Native>        parseNative(void);
    std::unique_ptr<AST::Switch>        parseSwitch(void);
    std::unique_ptr<AST::Function>      parseFunction(void);

public:
    std::unique_ptr<AST::Raise>         parseRaise(void);
    std::unique_ptr<AST::Delete>        parseDelete(void);
    std::unique_ptr<AST::Import>        parseImport(void);
    std::unique_ptr<AST::Function>      parseLambda(void);

/*** Control Flows ***/

public:
    std::unique_ptr<AST::Break>         parseBreak(void);
    std::unique_ptr<AST::Return>        parseReturn(void);
    std::unique_ptr<AST::Continue>      parseContinue(void);

/*** Object Modifiers ***/

public:
    std::unique_ptr<AST::Slice>         parseSlice(void);
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
    std::unique_ptr<AST::Unpack>            parseUnpack(Token::Operator terminator, bool tryParse);
    std::unique_ptr<AST::Literal>           parseLiteral(void);
    std::unique_ptr<AST::Composite>         parseComposite(CompositeSuggestion suggestion);
    std::unique_ptr<AST::Expression>        parseExpression(void);
    std::unique_ptr<AST::Expression>        parseReturnExpression(void);

private:
    bool isName(const std::unique_ptr<AST::Composite> &comp);
    bool isName(const std::unique_ptr<AST::Expression> &expr);

private:
    void pruneExpression(std::unique_ptr<AST::Expression> &expr);
    bool parseAssignTarget(AST::Assign::Target &target, Token::Operator terminator, bool tryParse);

public:
    std::unique_ptr<AST::Expression>        parseBoolOr(void);
    std::unique_ptr<AST::Expression>        parseBoolAnd(void);
    std::unique_ptr<AST::Expression>        parseBitOr(void);
    std::unique_ptr<AST::Expression>        parseBitXor(void);
    std::unique_ptr<AST::Expression>        parseBitAnd(void);
    std::unique_ptr<AST::Expression>        parseShifts(void);
    std::unique_ptr<AST::Expression>        parseAddSub(void);
    std::unique_ptr<AST::Expression>        parseTerm(void);
    std::unique_ptr<AST::Expression>        parsePower(void);

public:
    std::unique_ptr<AST::Expression>        parseCompares(void);
    std::unique_ptr<AST::Expression>        parseFactor(void);

/*** Generic Statements ***/

public:
    std::unique_ptr<AST::Statement>         parseStatement(void);
    std::unique_ptr<AST::CompoundStatement> parseCompondStatement(void);

};
}

#endif /* REDSCRIPT_COMPILER_PARSER_H */
