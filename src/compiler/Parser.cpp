#include "utils/Preprocessor.h"
#include "compiler/Parser.h"

namespace RedScript::Compiler
{
std::unique_ptr<AST::Node> Parser::parse(void)
{
    return std::unique_ptr<AST::Node>();
}

/*** Basic Language Structures ***/

std::unique_ptr<AST::Function> Parser::parseLambda(void)
{
    return std::unique_ptr<AST::Function>();
}

/*** Object Modifiers ***/

std::unique_ptr<AST::Index> Parser::parseIndex(void)
{
    return std::unique_ptr<AST::Index>();
}

std::unique_ptr<AST::Invoke> Parser::parseInvoke(void)
{
    return std::unique_ptr<AST::Invoke>();
}

std::unique_ptr<AST::Attribute> Parser::parseAttribute(void)
{
    return std::unique_ptr<AST::Attribute>();
}

/*** Expressions ***/

std::unique_ptr<AST::Name> Parser::parseName(void)
{
    Token::Ptr token = _lexer->next();
    std::unique_ptr<AST::Name> result(new AST::Name(token));

    /* extract identifier name */
    result->name = std::move(token->asIdentifier());
    return result;
}

std::unique_ptr<AST::Unpack> Parser::parseUnpack(void)
{
    return std::unique_ptr<AST::Unpack>();
}

std::unique_ptr<AST::Literal> Parser::parseLiteral(void)
{
    /* locate next token */
    Token::Ptr token = _lexer->next();
    Token::Type tokenType = token->type();

    /* check token type */
    switch (tokenType)
    {
        case Token::Type::Float   : return std::make_unique<AST::Literal>(token, token->asFloat());
        case Token::Type::String  : return std::make_unique<AST::Literal>(token, token->asString());
        case Token::Type::Integer : return std::make_unique<AST::Literal>(token, token->asInteger());

        case Token::Type::Eof:
        case Token::Type::Keywords:
        case Token::Type::Operators:
        case Token::Type::Identifiers:
            throw Runtime::SyntaxError(token);
    }
}

std::unique_ptr<AST::Composite> Parser::parseComposite(void)
{
    /* locate next token */
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Composite> result;

    /* the root value */
    switch (token->type())
    {
        case Token::Type::Eof:
        case Token::Type::Keywords:
            throw Runtime::SyntaxError(token);

        /* literal constants */
        case Token::Type::Float:
        case Token::Type::String:
        case Token::Type::Integer:
        {
            result = std::make_unique<AST::Composite>(token, parseLiteral());
            break;
        }

        /* identifier names, also maybe a lambda function that has only one argument */
        case Token::Type::Identifiers:
        {
            /* save the tokenizer state */
            _lexer->pushState();
            std::unique_ptr<AST::Name> name = parseName();
            Token::Ptr next = _lexer->peek();

            /* check if the following token is a '->' operator */
            if (next->isOperator<Token::Operator::Lambda>())
            {
                /* if so, it's a lambda function, so revert the tokenizer, and hand over to the lambda parser */
                _lexer->popState();
                result = std::make_unique<AST::Composite>(token, parseLambda());
            }
            else
            {
                /* no, it's just a simple name, so just commit the tokenizer and build the composite name node */
                _lexer->preserveState();
                result = std::make_unique<AST::Composite>(token, std::move(name));
            }

            break;
        }

        /* map or array expressions */
        case Token::Type::Operators:
        {
            switch (token->asOperator())
            {
                /* array expression */
                case Token::Operator::IndexLeft:
                {
                    /* array expression node */
                    std::unique_ptr<AST::Array> array(new AST::Array(token));

                    /* iterate until meets `IndexRight` token */
                    while (!((token = _lexer->next())->isOperator<Token::Operator::IndexRight>()))
                    {
                        /* parse an array item */
                        array->items.emplace_back(parseExpression());
                        token = _lexer->next();

                        /* check for comma seperator */
                        if (!(token->isOperator<Token::Operator::Comma>()))
                            if (!(token->isOperator<Token::Operator::IndexRight>()))
                                throw Runtime::SyntaxError(token, "Operator \",\" or \"]\" expected");
                    }

                    /* make it an array */
                    result = std::make_unique<AST::Composite>(token, std::move(array));
                    break;
                }

                /* map expression */
                case Token::Operator::BlockLeft:
                {
                    /* map expression node */
                    std::unique_ptr<AST::Map> map(new AST::Map(token));
                    std::unique_ptr<AST::Expression> key;
                    std::unique_ptr<AST::Expression> value;

                    /* iterate until meets `BlockRight` token */
                    while (!((token = _lexer->next())->isOperator<Token::Operator::BlockRight>()))
                    {
                        /* parse key and value */
                        key = parseExpression();
                        _lexer->operatorExpected<Token::Operator::Colon>();
                        value = parseExpression();

                        /* add to may key-value list */
                        map->items.emplace_back(std::make_pair(std::move(key), std::move(value)));
                        token = _lexer->next();

                        /* check for comma seperator */
                        if (!(token->isOperator<Token::Operator::Comma>()))
                            if (!(token->isOperator<Token::Operator::BlockRight>()))
                                throw Runtime::SyntaxError(token, "Operator \",\" or \"}\" expected");
                    }

                    /* make it a map */
                    result = std::make_unique<AST::Composite>(token, std::move(map));
                    break;
                }

                /* tuple expression, or lambda function */
                case Token::Operator::BracketLeft:
                {
                    // TODO: parse this crap
                }

                /* other operators are not allowed */
                default:
                    throw Runtime::SyntaxError(token);
            }

            break;
        }
    }

    /* may have modifiers */
    while ((token = _lexer->peekOrLine())->is<Token::Type::Operators>())
    {
        switch (token->asOperator())
        {
            case Token::Operator::Point: break;
            case Token::Operator::Lambda: break;
            case Token::Operator::IndexLeft: break;
            case Token::Operator::BlockLeft: break;
            case Token::Operator::BracketLeft: break;

            /* unknown operator, should terminate the modifier chain */
            default:
                return result;
        }
    }

    return result;
}

std::unique_ptr<AST::Expression> Parser::parseExpression(void)
{
    /* parse the whole expression, then prune out redundent nodes */
    std::unique_ptr<AST::Expression> result = parseContains();
    pruneExpression(result);
    return result;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "InfiniteRecursion"

bool Parser::isName(const std::unique_ptr<AST::Composite> &comp)
{
    /* must not containing any modifiers, and of course must be a name */
    return comp->mods.empty() && (comp->vtype == AST::Composite::ValueType::Name);
}

bool Parser::isName(const std::unique_ptr<AST::Expression> &expr)
{
    /* must not have unary operator, must not have any following operators, and the first operator must be a name */
    return !expr->hasOp && expr->follows.empty() &&
           (((expr->first.type == AST::Expression::Operand::Type::Composite) && isName(expr->first.composite)) ||
            ((expr->first.type == AST::Expression::Operand::Type::Expression) && isName(expr->first.expression)));
}

#pragma clang diagnostic pop

void Parser::pruneExpression(std::unique_ptr<AST::Expression> &expr)
{
    /* check whether the first operand is a sub-expression */
    if (expr->first.type == AST::Expression::Operand::Type::Expression)
    {
        /* if so, prune it recursively */
        pruneExpression(expr->first.expression);

        /* if the sub-expression doesn't have following operands,
         * and it's first operand has no operators, it's a "singular" node,
         * we can safely promote it's left-hand side operand to upper level */
        if (!expr->first.expression->hasOp && expr->first.expression->follows.empty())
        {
            expr->first.type = expr->first.expression->first.type;
            expr->first.composite = std::move(expr->first.expression->first.composite);
            expr->first.expression = std::move(expr->first.expression->first.expression);
        }
    }

    /* expression has no following operands,
     * and it's first operand is an expression and has no operands,
     * then it's a "bypass" node, so promotes to upper level */
    if (!expr->hasOp && expr->follows.empty() && (expr->first.type == AST::Expression::Operand::Type::Expression))
        expr = std::move(expr->first.expression);

    /* prune following operands recursively */
    for (auto &term : expr->follows)
        if (term.type == AST::Expression::Operand::Type::Expression)
            pruneExpression(term.expression);
}

#define MAKE_CASE_ITEM(_, op)       case Token::Operator::op:
#define MAKE_OPERATOR_LIST(...)     RSPP_FOR_EACH(MAKE_CASE_ITEM, ?, __VA_ARGS__)

#define MAKE_EXPRESSION_PARSER(TermType, ...)                                                           \
    {                                                                                                   \
        /* parse first operand of the expression */                                                     \
        Token::Ptr token = _lexer->peek();                                                              \
        std::unique_ptr<AST::Expression> result(new AST::Expression(token, parse ## TermType()));       \
                                                                                                        \
        /* check for consecutive operands */                                                            \
        for (token = _lexer->peek(); token->is<Token::Type::Operators>(); token = _lexer->peek())       \
        {                                                                                               \
            /* check the operator */                                                                    \
            switch (token->asOperator())                                                                \
            {                                                                                           \
                /* encountered the required operators,                                                  \
                 * parse the following operands of the expression */                                    \
                MAKE_OPERATOR_LIST(__VA_ARGS__)                                                         \
                {                                                                                       \
                    _lexer->next();                                                                     \
                    result->follows.emplace_back(token->asOperator(), parse ## TermType());             \
                    break;                                                                              \
                }                                                                                       \
                                                                                                        \
                /* not the operator we want */                                                          \
                default:                                                                                \
                    return result;                                                                      \
            }                                                                                           \
                                                                                                        \
            /* peek next token */                                                                       \
            token = _lexer->peek();                                                                     \
        }                                                                                               \
                                                                                                        \
        return result;                                                                                  \
    }

std::unique_ptr<AST::Expression> Parser::parseContains(void)    MAKE_EXPRESSION_PARSER(BoolOr   , In                                      )
std::unique_ptr<AST::Expression> Parser::parseBoolOr(void)      MAKE_EXPRESSION_PARSER(BoolAnd  , BoolOr                                  )
std::unique_ptr<AST::Expression> Parser::parseBoolAnd(void)     MAKE_EXPRESSION_PARSER(BitOr    , BoolAnd                                 )
std::unique_ptr<AST::Expression> Parser::parseBitOr(void)       MAKE_EXPRESSION_PARSER(BitXor   , BitOr                                   )
std::unique_ptr<AST::Expression> Parser::parseBitXor(void)      MAKE_EXPRESSION_PARSER(BitAnd   , BitXor                                  )
std::unique_ptr<AST::Expression> Parser::parseBitAnd(void)      MAKE_EXPRESSION_PARSER(Equals   , BitAnd                                  )
std::unique_ptr<AST::Expression> Parser::parseEquals(void)      MAKE_EXPRESSION_PARSER(Compares , Equ       , Neq                         )
std::unique_ptr<AST::Expression> Parser::parseCompares(void)    MAKE_EXPRESSION_PARSER(Shifts   , Leq       , Geq       , Less  , Greater )
std::unique_ptr<AST::Expression> Parser::parseShifts(void)      MAKE_EXPRESSION_PARSER(AddSub   , ShiftLeft , ShiftRight                  )
std::unique_ptr<AST::Expression> Parser::parseAddSub(void)      MAKE_EXPRESSION_PARSER(Term     , Plus      , Minus                       )
std::unique_ptr<AST::Expression> Parser::parseTerm(void)        MAKE_EXPRESSION_PARSER(Power    , Multiply  , Divide    , Module          )
std::unique_ptr<AST::Expression> Parser::parsePower(void)       MAKE_EXPRESSION_PARSER(Factor   , Power                                   )

#undef MAKE_CASE_ITEM
#undef MAKE_OPERATOR_LIST
#undef MAKE_EXPRESSION_PARSER

std::unique_ptr<AST::Expression> Parser::parseFactor(void)
{
    /* locate next token */
    Token::Ptr token = _lexer->peek();
    Token::Type tokenType = token->type();

    /* check for token type */
    switch (tokenType)
    {
        case Token::Type::Eof:
        case Token::Type::Keywords:
            throw Runtime::SyntaxError(token);

        case Token::Type::Float:
        case Token::Type::String:
        case Token::Type::Integer:
        case Token::Type::Identifiers:
        {
            /* composite types (constants, names, arrays, maps, tuples, lambdas with attributes, indexes, and invokes) */
            return std::make_unique<AST::Expression>(token, parseComposite());
        }

        case Token::Type::Operators:
        {
            switch (token->asOperator())
            {
                /* unary operators */
                case Token::Operator::Plus:
                case Token::Operator::Minus:
                case Token::Operator::BitNot:
                case Token::Operator::BoolNot:
                {
                    _lexer->next();
                    return std::make_unique<AST::Expression>(token, parseFactor(), token->asOperator());
                }

                /* map or array expressions */
                case Token::Operator::BlockLeft:
                case Token::Operator::IndexLeft:
                {
                    /* composite types (constants, names, arrays, maps, tuples, lambdas with attributes, indexes, and invokes) */
                    return std::make_unique<AST::Expression>(token, parseComposite());
                }

                /* nested expression, or a tuple, or even a lambda, so handle it seperately */
                case Token::Operator::BracketLeft:
                {
                    int n = 0;
                    bool hasTail;
                    bool nameOnly = true;

                    /* save the state, then skip the '(' operator */
                    _lexer->pushState();
                    _lexer->next();

                    /* try locate the next token */
                    Token::Ptr next = _lexer->peek();
                    std::unique_ptr<AST::Expression> expr;

                    /* may be empty tuples or lambda functions with no arguments,
                     * but in either case, the composite parser will take over it */
                    if (next->isOperator<Token::Operator::BracketRight>())
                    {
                        _lexer->popState();
                        return std::make_unique<AST::Expression>(token, parseComposite());
                    }

                    do
                    {
                        /* '*' and '**' prefix operators encountered, definately a lambda function */
                        if (next->isOperator<Token::Operator::Power>() || next->isOperator<Token::Operator::Multiply>())
                        {
                            _lexer->popState();
                            return std::make_unique<AST::Expression>(token, parseComposite());
                        }

                        /* parse next expression */
                        n++;
                        expr = parseExpression();
                        next = _lexer->peek();
                        hasTail = false;
                        nameOnly &= isName(expr);

                        /* meets `BracketRight` token, the sequence is over */
                        if (next->isOperator<Token::Operator::BracketRight>())
                            break;

                        /* skip the comma seperator */
                        if (!(_lexer->next()->isOperator<Token::Operator::Comma>()))
                            throw Runtime::SyntaxError(token, "Operator \",\" or \")\" expected");

                        /* once encountered a comma, the result may never be a nested expression */
                        next = _lexer->peek();
                        hasTail = true;

                    /* iterate until meets `BracketRight` token */
                    } while (!(next->isOperator<Token::Operator::BracketRight>()));

                    /* it has a tail comma, definately a tuple */
                    if (hasTail)
                    {
                        _lexer->popState();
                        return std::make_unique<AST::Expression>(token, parseComposite());
                    }

                    /* skip the right bracket */
                    _lexer->next();
                    next = _lexer->peek();

                    /* check whether it maybe a lambda function, which must
                     * not have the tail comma, and all arguments must be names-only;
                     * or there are more than one item, it's definately a tuple;
                     * but in either case, the composite parser will take over it */
                    if ((n > 1) || (nameOnly && next->isOperator<Token::Operator::Lambda>()))
                    {
                        _lexer->popState();
                        return std::make_unique<AST::Expression>(token, parseComposite());
                    }

                    /* otherwise, it's just a simple nested expression */
                    _lexer->preserveState();
                    return std::move(expr);
                }

                default:
                    throw Runtime::SyntaxError(token);
            }
        }
    }
}
}
