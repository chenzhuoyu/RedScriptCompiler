#include "utils/Preprocessor.h"
#include "compiler/Parser.h"

namespace RedScript::Compiler
{
std::unique_ptr<AST::Node> Parser::parse(void)
{
    return std::unique_ptr<AST::Node>();
}

/*** Expressions ***/

std::unique_ptr<AST::Name> Parser::parseName(void)
{
    return std::unique_ptr<AST::Name>();
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
    Token::Ptr token = _lexer->next();
    std::unique_ptr<AST::Composite> result;

    /* the root value */
    switch (token->type())
    {
        case Token::Type::Eof:
        case Token::Type::Keywords:
        case Token::Type::Operators:
            throw Runtime::SyntaxError(token);

        case Token::Type::Float:
        case Token::Type::String:
        case Token::Type::Integer:
        {
            /* literal constants */
            result = std::make_unique<AST::Composite>(token, parseLiteral());
            break;
        }

        case Token::Type::Identifiers:
        {
            /* identifier names */
            result = std::make_unique<AST::Composite>(token, parseName());
            break;
        }
    }

    /* may have modifiers */
    while ((token = _lexer->peekOrLine())->is<Token::Type::Operators>())
    {
        switch (token->asOperator())
        {
            case Token::Operator::Point: break;
            case Token::Operator::Tuple: break;
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

void Parser::pruneExpression(std::unique_ptr<AST::Expression> &expr)
{
    /* prune right-hand side expressions if any */
    if (expr->right)
        pruneExpression(expr->right);

    /* check whether the left-hand side is a sub-expression */
    if (expr->left && expr->left->expression)
    {
        /* if so, prune it recursively */
        pruneExpression(expr->left->expression);

        /* if the sub-expression doesn't have right-hand side operand,
         * we can safely promote it's left-hand side operand to upper level */
        if (!(expr->left->expression->right))
            expr->left = std::move(expr->left->expression->left);

        /* expression has no right-hand side operand,
         * and it's left-hand side operand is an expression,
         * then it's a "bypass" node, so promotes to upper level */
        if (!(expr->right) && expr->left->expression)
            expr = std::move(expr->left->expression);
    }
}

#define MAKE_CASE_ITEM(_, op)       case Token::Operator::op:
#define MAKE_OPERATOR_LIST(...)     RSPP_FOR_EACH(MAKE_CASE_ITEM, ?, __VA_ARGS__)

#define MAKE_EXPRESSION_PARSER(TermType, ...)                                                               \
    {                                                                                                       \
        /* parse left-hand side expression */                                                               \
        Token::Ptr token = _lexer->peek();                                                                  \
        std::unique_ptr<AST::Expression> result(new AST::Expression(token, parse ## TermType()));           \
                                                                                                            \
        /* check for consecutive operators */                                                               \
        while (token->is<Token::Type::Operators>())                                                         \
        {                                                                                                   \
            /* check the operator */                                                                        \
            switch (token->asOperator())                                                                    \
            {                                                                                               \
                /* encountered the required operators,                                                      \
                 * save the operator and parse the right-hand side of the expression */                     \
                MAKE_OPERATOR_LIST(__VA_ARGS__)                                                             \
                {                                                                                           \
                    /* operators are left-combined, so we need to left-chain those terms */                 \
                    Token::Ptr next = _lexer->next();                                                       \
                    std::unique_ptr<AST::Expression> right(new AST::Expression(next, std::move(result)));   \
                                                                                                            \
                    /* parse the right-hand side operand */                                                 \
                    right->op = next->asOperator();                                                         \
                    right->right = parse ## TermType();                                                     \
                                                                                                            \
                    /* replace the original node with new chain head */                                     \
                    result = std::move(right);                                                              \
                    break;                                                                                  \
                }                                                                                           \
                                                                                                            \
                /* not the operator we want */                                                              \
                default:                                                                                    \
                    return result;                                                                          \
            }                                                                                               \
                                                                                                            \
            /* peek next token */                                                                           \
            _lexer->next();                                                                                 \
            token = _lexer->peek();                                                                         \
        }                                                                                                   \
                                                                                                            \
        return result;                                                                                      \
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
            /* composite types (constants, names, attributes, indexes, and invokes) */
            return std::make_unique<AST::Expression>(token, parseComposite());
        }

        case Token::Type::Operators:
        {
            /* read and skip the operator */
            Token::Ptr next = _lexer->next();
            Token::Operator op = next->asOperator();

            switch (op)
            {
                case Token::Operator::Plus:
                case Token::Operator::Minus:
                case Token::Operator::BitNot:
                case Token::Operator::BoolNot:
                {
                    /* unary operators before actual factor */
                    return std::make_unique<AST::Expression>(next, parseFactor(), op);
                }

                case Token::Operator::BracketLeft:
                {
                    /* nested expression, gives this node directly */
                    std::unique_ptr<AST::Expression> result = parseExpression();
                    _lexer->operatorExpected<Token::Operator::BracketRight>();
                    return result;
                }

                default:
                    throw Runtime::SyntaxError(token);
            }
        }
    }
}
}
