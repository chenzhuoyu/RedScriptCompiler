#include "utils/Strings.h"
#include "utils/Preprocessor.h"
#include "compiler/Parser.h"
#include "exceptions/SyntaxError.h"

namespace RedScript::Compiler
{
std::unique_ptr<AST::CompoundStatement> Parser::parse(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::CompoundStatement> result(new AST::CompoundStatement(token));

    /* parse each statement until end of file */
    while (!(token->is<Token::Type::Eof>()))
    {
        result->statements.emplace_back(parseStatement());
        token = _lexer->peek();
    }

    return result;
}

/*** Basic Language Structures ***/

std::unique_ptr<AST::If> Parser::parseIf(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::If> result(new AST::If(token));

    /* if (<cond>) <stmt> */
    _lexer->keywordExpected<Token::Keyword::If>();
    _lexer->operatorExpected<Token::Operator::BracketLeft>();
    result->expr = parseExpression();
    _lexer->operatorExpected<Token::Operator::BracketRight>();
    result->positive = parseStatement();

    /* optional else <stmt> */
    if (_lexer->peek()->isKeyword<Token::Keyword::Else>())
    {
        _lexer->next();
        result->negative = parseStatement();
    }

    return result;
}

std::unique_ptr<AST::For> Parser::parseFor(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::For> result(new AST::For(token));

    /* for (<sequence> in <expr>) <body> */
    _lexer->keywordExpected<Token::Keyword::For>();
    _lexer->operatorExpected<Token::Operator::BracketLeft>();
    parseAssignTarget(result->comp, result->pack, Token::Operator::In);
    result->expr = parseExpression();
    _lexer->operatorExpected<Token::Operator::BracketRight>();
    result->body = Scope::wrap(_loops, std::bind(&Parser::parseStatement, this));

    /* optional else <branch> */
    if (_lexer->peek()->isKeyword<Token::Keyword::Else>())
    {
        _lexer->next();
        result->branch = parseStatement();
    }

    return result;
}

std::unique_ptr<AST::Try> Parser::parseTry(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Try> result(new AST::Try(token));

    /* try <stmt> */
    _lexer->keywordExpected<Token::Keyword::Try>();
    result->body = parseStatement();

    /* parse every "except" section */
    while (_lexer->peek()->isKeyword<Token::Keyword::Except>())
    {
        AST::Try::Except except;

        /* except (<expr>) */
        _lexer->next();
        _lexer->operatorExpected<Token::Operator::BracketLeft>();
        except.exception = parseExpression();

        /* optional exception alias */
        if (_lexer->peek()->isKeyword<Token::Keyword::As>())
        {
            _lexer->next();
            except.alias = parseName();
        }

        /* parse handler */
        _lexer->operatorExpected<Token::Operator::BracketRight>();
        except.handler = parseStatement();
        result->excepts.emplace_back(std::move(except));
    }

    /* optional finally section */
    if (_lexer->peek()->isKeyword<Token::Keyword::Finally>())
    {
        _lexer->next();
        result->finally = parseStatement();
    }

    /* should have at least one of "finally" or "catch" section */
    if (!(result->finally) && result->excepts.empty())
        throw Exceptions::SyntaxError(token, "\"except\" or \"finally\" expected");
    else
        return result;
}

std::unique_ptr<AST::Class> Parser::parseClass(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Class> result(new AST::Class(token));

    /* class <name> */
    _lexer->keywordExpected<Token::Keyword::Class>();
    result->name = parseName();

    /* optional super class */
    if (_lexer->peek()->isOperator<Token::Operator::Colon>())
    {
        _lexer->next();
        result->super = parseExpression();
    }

    /* class body */
    result->body = parseStatement();
    return result;
}

std::unique_ptr<AST::While> Parser::parseWhile(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::While> result(new AST::While(token));

    /* while (<expr>) <body> */
    _lexer->keywordExpected<Token::Keyword::While>();
    _lexer->operatorExpected<Token::Operator::BracketLeft>();
    result->expr = parseExpression();
    _lexer->operatorExpected<Token::Operator::BracketRight>();
    result->body = Scope::wrap(_loops, std::bind(&Parser::parseStatement, this));

    /* optional else <branch> */
    if (_lexer->peek()->isKeyword<Token::Keyword::Else>())
    {
        _lexer->next();
        result->branch = parseStatement();
    }

    return result;
}

std::unique_ptr<AST::Native> Parser::parseNative(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Native> result(new AST::Native(token));

    /* native '<lang>' */
    if (!(_lexer->next()->isKeyword<Token::Keyword::Native>()))
        throw Exceptions::SyntaxError(token, "Keyword \"native\" expected");

    /* must be a string */
    std::string lang = _lexer->next()->asString();
    Utils::Strings::lower(lang);

    /* class <name> (<options>) */
    _lexer->keywordExpected<Token::Keyword::Class>();
    result->name = parseName();
    _lexer->operatorExpected<Token::Operator::BracketLeft>();

    /* parse each options, if any */
    if (!(_lexer->peek()->isOperator<Token::Operator::BracketRight>()))
    {
        for (;;)
        {
            AST::Native::Option option;

            /* <name> = <value> */
            option.name = parseName();
            _lexer->operatorExpected<Token::Operator::Assign>();
            option.value = parseExpression();

            /* check for end of options */
            if (_lexer->peek()->isOperator<Token::Operator::BracketRight>())
            {
                result->opts.emplace_back(std::move(option));
                break;
            }

            /* comma required if options not end */
            result->opts.emplace_back(std::move(option));
            _lexer->operatorExpected<Token::Operator::Comma>();
        }
    }

    /* skip the ")" operator */
    _lexer->next();
    _lexer->operatorExpected<Token::Operator::BlockLeft>();

    /* currently only supports C language */
    if (lang != "c")
        throw Exceptions::SyntaxError(token, "Only \"C\" is supported for now");

    /* bracket counter */
    int bc = 0;
    char ch = 0;
    char next = 0;

    /* pad with space, for better error message */
    result->code = std::string(static_cast<size_t>(_lexer->row() - 1), '\n');
    result->code += std::string(static_cast<size_t>(_lexer->col() - 1), ' ');

    /* parse C code in raw mode */
    for (;;)
    {
        /* check the character */
        switch ((next = _lexer->peekChar(true)))
        {
            /* EOF */
            case 0:
                throw Exceptions::SyntaxError(_lexer, "EOF occured when parsing native C code");

            /* count brackets */
            case '{': bc += 1; break;
            case '}': bc -= 1; break;

            /* character and string literals  */
            case '\'':
            case '\"':
            {
                /* add to source code */
                _lexer->nextChar(true);
                result->code += next;

                /* append every character in string as-is */
                while ((ch = _lexer->nextChar(true)) != next)
                {
                    /* check for EOF */
                    if (!ch)
                        throw Exceptions::SyntaxError(_lexer, "EOF occured when parsing native C code");

                    /* check for escape character */
                    if (ch != '\\')
                    {
                        result->code += ch;
                        continue;
                    }

                    /* check for EOF */
                    if (!(ch = _lexer->nextChar(true)))
                        throw Exceptions::SyntaxError(_lexer, "EOF occured when parsing native C code");

                    /* append to source too */
                    result->code += '\\';
                    result->code += ch;
                }

                /* and the closing character */
                result->code += next;
                continue;
            }

            /* may be comment start */
            case '/':
            {
                /* add to source code */
                _lexer->nextChar(true);
                result->code += next;

                /* check for next character */
                switch ((ch = _lexer->nextChar(true)))
                {
                    /* EOF */
                    case 0:
                        throw Exceptions::SyntaxError(_lexer, "EOF occured when parsing native C code");

                    /* single line comment */
                    case '/':
                    {
                        do
                        {
                            result->code += ch;
                            ch = _lexer->nextChar(true);
                        } while (ch && (ch != '\r') && (ch != '\n'));

                        /* check for character */
                        if (!ch)
                            throw Exceptions::SyntaxError(_lexer, "EOF occured when parsing native C code");

                        /* add line feed */
                        result->code += ch;
                        continue;
                    }

                    /* block comment */
                    case '*':
                    {
                        for (;;)
                        {
                            result->code += ch;
                            ch = _lexer->nextChar(true);

                            /* check for character */
                            if (!ch)
                                throw Exceptions::SyntaxError(_lexer, "EOF occured when parsing native C code");

                            /* maybe end of block comment */
                            if (ch == '*')
                            {
                                if (!(ch = _lexer->nextChar(true)))
                                    throw Exceptions::SyntaxError(_lexer, "EOF occured when parsing native C code");

                                /* indeed an end of block comment */
                                if (ch == '/')
                                {
                                    result->code += "*/";
                                    break;
                                }

                                /* not, append the star to source */
                                result->code += '*';
                            }
                        }

                        continue;
                    }

                    /* other characters, add to source code */
                    default:
                    {
                        result->code += ch;
                        continue;
                    }
                }
            }

            /* other characters, append to source by default */
            default:
                break;
        }

        /* more bracket than needed, the native source ended */
        if (bc < 0)
            break;

        /* add to source code */
        _lexer->nextChar(true);
        result->code += next;
    }

    /* check for end of native code */
    _lexer->operatorExpected<Token::Operator::BlockRight>();
    return result;
}

std::unique_ptr<AST::Switch> Parser::parseSwitch(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Switch> result(new AST::Switch(token));

    /* switch (<expr>) { <cases> } */
    _lexer->keywordExpected<Token::Keyword::Switch>();
    _lexer->operatorExpected<Token::Operator::BracketLeft>();
    result->expr = parseExpression();
    _lexer->operatorExpected<Token::Operator::BracketRight>();
    _lexer->operatorExpected<Token::Operator::BlockLeft>();

    /* parse each case */
    while (!((token = _lexer->peek())->isOperator<Token::Operator::BlockRight>()))
    {
        switch (token->asKeyword())
        {
            case Token::Keyword::Case:
            {
                /* permit for using "break" */
                Scope _(_cases);
                AST::Switch::Case item;

                /* default section must be the last section */
                if (result->def)
                    throw Exceptions::SyntaxError(token, "Can't place \"case\" after \"default\"");

                /* case <value>: <stmt> */
                _lexer->next();
                item.value = parseExpression();
                _lexer->operatorExpected<Token::Operator::Colon>();
                token = _lexer->peek();

                /* optional case body */
                if (!(token->isKeyword<Token::Keyword::Case>()) && !(token->isKeyword<Token::Keyword::Default>()))
                    item.body = parseStatement();

                /* add to cases */
                result->cases.emplace_back(std::move(item));
                break;
            }

            case Token::Keyword::Default:
            {
                /* permit for using "break" */
                Scope _(_cases);

                /* can only have at most 1 default section */
                if (result->def)
                    throw Exceptions::SyntaxError(token, "Duplicated \"default\" section");

                /* default: <stmt> */
                _lexer->next();
                _lexer->operatorExpected<Token::Operator::Colon>();
                result->def = parseStatement();
                break;
            }

            default:
                throw Exceptions::SyntaxError(token, "Keyword \"case\" or \"default\" expected");
        }
    }

    /* must have at least 1 case */
    if (result->cases.empty())
        throw Exceptions::SyntaxError(token, "Switch without any cases is not allowed");

    /* skip the block right operator */
    _lexer->next();
    return result;
}

std::unique_ptr<AST::Function> Parser::parseFunction(void)
{
    /* reset "case" counter, loop counter, and enter a new function scope */
    Reset cases(_cases);
    Reset loops(_loops);
    Scope functions(_functions);

    /* peek the next token */
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Function> result(new AST::Function(token));

    /* function name */
    _lexer->keywordExpected<Token::Keyword::Function>();
    result->name = parseName();

    /* it must starts with '(' operator */
    _lexer->operatorExpected<Token::Operator::BracketLeft>();
    token = _lexer->peek();

    /* iterate until meets `BracketRight` token */
    if (!(token->isOperator<Token::Operator::BracketRight>()))
    {
        for (;;)
        {
            /* '**' keyword argument prefix operator */
            if (token->isOperator<Token::Operator::Power>())
            {
                /* cannot have more than one keyword argument */
                if (result->kwargs)
                    throw Exceptions::SyntaxError(token, "Cannot have more than one keyword argument");

                /* parse as keyword argument */
                _lexer->next();
                result->kwargs = parseName();
                token = _lexer->peek();
            }

            /* keyword argument must be the last argument */
            else if (result->kwargs)
                throw Exceptions::SyntaxError(token, "Keyword argument must be the last argument");

            /* '*' varidic argument prefix operator */
            else if (token->isOperator<Token::Operator::Multiply>())
            {
                /* cannot have more than one varidic argument */
                if (result->vargs)
                    throw Exceptions::SyntaxError(token, "Cannot have more than one varidic argument");

                /* parse as variable arguments */
                _lexer->next();
                result->vargs = parseName();
                token = _lexer->peek();
            }

            /* varidic argument must be the last argument but before keyword argument */
            else if (result->vargs)
                throw Exceptions::SyntaxError(token, "Varidic argument must be the last argument but before keyword argument");

            /* just a simple name */
            else
            {
                result->args.emplace_back(parseName());
                token = _lexer->peek();

                /* may have default value */
                if (token->isOperator<Token::Operator::Assign>())
                {
                    _lexer->next();
                    result->defaults.emplace_back(parseExpression());
                    token = _lexer->peek();
                }

                /* default values must be placed after normal arguments */
                else if (!(result->defaults.empty()))
                    throw Exceptions::SyntaxError(token, "Default values must be placed after normal arguments");
            }

            /* ')' encountered, argument list terminated */
            if (token->isOperator<Token::Operator::BracketRight>())
                break;

            /* ',' encountered, followed by more arguments  */
            else if (token->isOperator<Token::Operator::Comma>())
            {
                _lexer->next();
                token = _lexer->peek();
            }

            /* otherwise it's an error */
            else
                throw Exceptions::SyntaxError(token);
        }
    }

    /* skip the ')', and parse statement body */
    _lexer->next();
    result->body = parseStatement();
    return result;
}

/*** Misc. Statements ***/

std::unique_ptr<AST::Raise> Parser::parseRaise(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Raise> result(new AST::Raise(token));

    /* raise <expr> */
    _lexer->keywordExpected<Token::Keyword::Raise>();
    result->expr = parseExpression();
    return result;
}

std::unique_ptr<AST::Delete> Parser::parseDelete(void)
{
    Token::Ptr token = _lexer->next();
    std::unique_ptr<AST::Delete> result(new AST::Delete(token));

    /* check for "delete" keyword */
    if (!(token->isKeyword<Token::Keyword::Delete>()))
        throw Exceptions::SyntaxError(token, "Keyword \"delete\" expected");

    /* parse as expression */
    std::unique_ptr<AST::Composite> comp = nullptr;
    std::unique_ptr<AST::Expression> expr = parseExpression();

    /* should be a single `Composite`, so test and extract it */
    while (!comp)
    {
        /* has unary operators or following operands, it's an expression, and not assignable */
        if (expr->hasOp || !expr->follows.empty())
            throw Exceptions::SyntaxError(token, "Expressions are not deletable");

        /* if it's the composite, extract it, otherwise, move to inner expression */
        if (expr->first.type == AST::Expression::Operand::Type::Composite)
            comp = std::move(expr->first.composite);
        else
            expr = std::move(expr->first.expression);
    }

    /* and if it's mutable, take it directly */
    if (!(comp->isSyntacticallyMutable(true)))
        throw Exceptions::SyntaxError(token, "Expressions are not deletable");

    /* compiler is not smart enough to use move constructor */
    result->comp = std::move(comp);
    return result;
}

std::unique_ptr<AST::Import> Parser::parseImport(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Import> result(new AST::Import(token));

    /* check for "import" keyword */
    if (!(token->isKeyword<Token::Keyword::Import>()))
        throw Exceptions::SyntaxError(token, "Keyword \"import\" expected");

    /* parse all the name sections */
    do { _lexer->next(); result->names.emplace_back(parseName()); }
    while ((token = _lexer->peek())->isOperator<Token::Operator::Point>());

    /* may follows a "from" keyword */
    if (token->isKeyword<Token::Keyword::From>())
    {
        _lexer->next();
        result->from = parseExpression();
        token = _lexer->peek();
    }

    /* may also follows an "as" keyword */
    if (token->isKeyword<Token::Keyword::As>())
    {
        /* skip the "as" keyword, and parse the alias */
        _lexer->next();
        result->alias = parseName();
    }

    return result;
}

std::unique_ptr<AST::Function> Parser::parseLambda(void)
{
    /* reset "case" counter, loop counter, and enter a new function scope */
    Reset cases(_cases);
    Reset loops(_loops);
    Scope functions(_functions);

    /* peek the next token */
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Function> result(new AST::Function(token));

    /* lambda with one argument can be a simple name */
    if (token->is<Token::Type::Identifiers>())
    {
        result->args.emplace_back(parseName());
        result->name = nullptr;
        result->vargs = nullptr;
        result->kwargs = nullptr;
    }

    /* otherwise it's an argument list */
    else
    {
        /* it must starts with '(' operator */
        _lexer->operatorExpected<Token::Operator::BracketLeft>();
        token = _lexer->peek();

        /* iterate until meets `BracketRight` token */
        if (!(token->isOperator<Token::Operator::BracketRight>()))
        {
            for (;;)
            {
                /* '**' keyword argument prefix operator */
                if (token->isOperator<Token::Operator::Power>())
                {
                    /* cannot have more than one keyword argument */
                    if (result->kwargs)
                        throw Exceptions::SyntaxError(token, "Cannot have more than one keyword argument");

                    /* parse as keyword argument */
                    _lexer->next();
                    result->kwargs = parseName();
                    token = _lexer->peek();
                }

                /* keyword argument must be the last argument */
                else if (result->kwargs)
                    throw Exceptions::SyntaxError(token, "Keyword argument must be the last argument");

                /* '*' varidic argument prefix operator */
                else if (token->isOperator<Token::Operator::Multiply>())
                {
                    /* cannot have more than one varidic argument */
                    if (result->vargs)
                        throw Exceptions::SyntaxError(token, "Cannot have more than one varidic argument");

                    /* parse as variable arguments */
                    _lexer->next();
                    result->vargs = parseName();
                    token = _lexer->peek();
                }

                /* varidic argument must be the last argument but before keyword argument */
                else if (result->vargs)
                    throw Exceptions::SyntaxError(token, "Varidic argument must be the last argument but before keyword argument");

                /* just a simple name */
                else
                {
                    result->args.emplace_back(parseName());
                    token = _lexer->peek();

                    /* may have default value */
                    if (token->isOperator<Token::Operator::Assign>())
                    {
                        _lexer->next();
                        result->defaults.emplace_back(parseExpression());
                        token = _lexer->peek();
                    }

                    /* default values must be placed after normal arguments */
                    else if (!(result->defaults.empty()))
                        throw Exceptions::SyntaxError(token, "Default values must be placed after normal arguments");
                }

                /* ')' encountered, argument list terminated */
                if (token->isOperator<Token::Operator::BracketRight>())
                    break;

                /* ',' encountered, followed by more arguments  */
                else if (token->isOperator<Token::Operator::Comma>())
                {
                    _lexer->next();
                    token = _lexer->peek();
                }

                /* otherwise it's an error */
                else
                    throw Exceptions::SyntaxError(token);
            }
        }

        /* skip the ')' */
        _lexer->next();
    }

    /* must follows a lambda operator */
    _lexer->operatorExpected<Token::Operator::Lambda>();
    token = _lexer->peek();

    /* check whether it's a expression lambda */
    if (token->isOperator<Token::Operator::BlockLeft>())
    {
        /* no, parse as simple statement */
        result->name = nullptr;
        result->body = parseStatement();
    }
    else
    {
        /* yes, build a simple return statement */
        result->name = nullptr;
        result->body = std::make_unique<AST::Statement>(token, std::make_unique<AST::Return>(token, parseReturnExpression()));
    }

    return result;
}

/*** Control Flows ***/

std::unique_ptr<AST::Break> Parser::parseBreak(void)
{
    Token::Ptr token = _lexer->peek();

    if (!_cases && !_loops)
        throw Exceptions::SyntaxError(token, "`break` outside of loops or cases");

    _lexer->keywordExpected<Token::Keyword::Break>();
    return std::make_unique<AST::Break>(token);
}

std::unique_ptr<AST::Return> Parser::parseReturn(void)
{
    Token::Ptr token = _lexer->peek();

    if (!_functions)
        throw Exceptions::SyntaxError(token, "`return` outside of functions");

    _lexer->keywordExpected<Token::Keyword::Return>();
    return std::make_unique<AST::Return>(token, parseReturnExpression());
}

std::unique_ptr<AST::Continue> Parser::parseContinue(void)
{
    Token::Ptr token = _lexer->peek();

    if (!_loops)
        throw Exceptions::SyntaxError(token, "`continue` outside of loops");

    _lexer->keywordExpected<Token::Keyword::Continue>();
    return std::make_unique<AST::Continue>(token);
}

/*** Object Modifiers ***/

std::unique_ptr<AST::Slice> Parser::parseSlice(void)
{
    Token::Ptr token = _lexer->next();
    std::unique_ptr<AST::Slice> result(new AST::Slice(token));

    /* parse beginning expression if any */
    if (!(_lexer->peek()->isOperator<Token::Operator::Colon>()))
        result->begin = parseExpression();

    /* must be a colon here */
    _lexer->operatorExpected<Token::Operator::Colon>();
    token = _lexer->peek();

    /* meet "]", no ending and stepping expressions */
    if (token->isOperator<Token::Operator::IndexRight>())
    {
        _lexer->next();
        return result;
    }

    /* meet ":", no ending expression */
    if (!(token->isOperator<Token::Operator::Colon>()))
    {
        result->end = parseExpression();
        token = _lexer->peek();

        /* meet "]", no stepping expression */
        if (token->isOperator<Token::Operator::IndexRight>())
        {
            _lexer->next();
            return result;
        }
    }

    /* meet ":" again, may have stepping expression */
    if (token->isOperator<Token::Operator::Colon>())
    {
        /* skip the ":" */
        _lexer->next();
        token = _lexer->peek();

        /* meet "]", no stepping expression */
        if (token->isOperator<Token::Operator::IndexRight>())
        {
            _lexer->next();
            return result;
        }

        /* parse stepping expression */
        result->step = parseExpression();
    }

    /* must ends with "]" */
    _lexer->operatorExpected<Token::Operator::IndexRight>();
    return result;
}

std::unique_ptr<AST::Invoke> Parser::parseInvoke(void)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Invoke> result(new AST::Invoke(token));

    /* it must starts with '(' operator */
    _lexer->operatorExpected<Token::Operator::BracketLeft>();
    token = _lexer->peek();

    /* iterate until meets `BracketRight` token */
    while (!(token->isOperator<Token::Operator::BracketRight>()))
    {
        /* '**' keyword argument prefix operator */
        if (token->isOperator<Token::Operator::Power>())
        {
            /* cannot have more than one keyword argument */
            if (result->kwarg)
                throw Exceptions::SyntaxError(token, "Cannot have more than one keyword argument");

            /* parse as keyword argument */
            _lexer->next();
            result->kwarg = parseExpression();
            token = _lexer->peek();
        }

        /* keyword argument must be the last argument */
        else if (result->kwarg)
            throw Exceptions::SyntaxError(token, "Keyword argument must be the last argument");

        /* '*' varidic argument prefix operator */
        else if (token->isOperator<Token::Operator::Multiply>())
        {
            /* cannot have more than one varidic argument */
            if (result->varg)
                throw Exceptions::SyntaxError(token, "Cannot have more than one varidic argument");

            /* parse as variable arguments */
            _lexer->next();
            result->varg = parseExpression();
            token = _lexer->peek();
        }

        /* varidic argument must be the last argument but before keyword argument */
        else if (result->varg)
            throw Exceptions::SyntaxError(token, "Varidic argument must be the last argument but before keyword argument");

        /* a simple argument, or a named argument */
        else
        {
            /* try parsing as an expression */
            std::unique_ptr<AST::Name> name;
            std::unique_ptr<AST::Expression> expr = parseExpression();

            /* it's a pure name followed by a '=' operator, means it's a named argument */
            if (isName(expr) && _lexer->peek()->isOperator<Token::Operator::Assign>())
            {
                /* find the correct composite node in the expression chain */
                while (expr->first.type == AST::Expression::Operand::Type::Expression)
                    expr = std::move(expr->first.expression);

                /* add to result list */
                _lexer->next();
                result->kwargs.emplace_back(std::move(expr->first.composite->name), parseExpression());
                token = _lexer->peek();
            }

            /* nope, it's a simple argument, check it's order */
            else if (result->kwargs.empty())
            {
                result->args.emplace_back(std::move(expr));
                token = _lexer->peek();
            }

            /* named arguments must be placed after normal arguments */
            else
                throw Exceptions::SyntaxError(token, "Non-keyword argument after keyword argument");
        }

        /* check for the comma seperator */
        if (token->isOperator<Token::Operator::Comma>())
        {
            _lexer->next();
            token = _lexer->peek();
        }

        /* check for the `BracketRight` operator */
        else if (token->isOperator<Token::Operator::BracketRight>())
            break;

        /* otherwise it's an error */
        else
            throw Exceptions::SyntaxError(token, "Operator \",\" or \")\" expected");
    }

    /* skip the right bracket */
    _lexer->next();
    return result;
}

std::unique_ptr<AST::Attribute> Parser::parseAttribute(void)
{
    Token::Ptr token = _lexer->next();
    std::unique_ptr<AST::Attribute> result(new AST::Attribute(token));

    result->attr = parseName();
    return result;
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

std::unique_ptr<AST::Unpack> Parser::parseUnpack(Token::Operator terminator)
{
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Unpack> result(new AST::Unpack(token));

    /* iterate until meets the termination operator */
    for (;;)
    {
        /* parse as an expression */
        _lexer->pushState();
        std::unique_ptr<AST::Composite> comp = nullptr;
        std::unique_ptr<AST::Expression> expr = parseFactor();

        /* should be a single `Composite`, so test and extract it */
        while (!comp)
        {
            /* has unary operators or following operands, it's an expression, and not assignable */
            if (expr->hasOp || !expr->follows.empty())
                throw Exceptions::SyntaxError(token, "Expressions are not assignable");

            /* if it's the composite, extract it, otherwise, move to inner expression */
            if (expr->first.type == AST::Expression::Operand::Type::Composite)
                comp = std::move(expr->first.composite);
            else
                expr = std::move(expr->first.expression);
        }

        /* and if it's mutable, take it directly */
        if (comp->isSyntacticallyMutable(true))
        {
            token = _lexer->peek();
            _lexer->preserveState();
            result->items.emplace_back(std::move(comp));
        }

        /* otherwise, it should be a tuple or array composite, which could turns into a nested `Unpack` */
        else
        {
            switch (comp->vtype)
            {
                /* they are all immutable */
                case AST::Composite::ValueType::Map:
                case AST::Composite::ValueType::Name:
                case AST::Composite::ValueType::Literal:
                case AST::Composite::ValueType::Function:
                case AST::Composite::ValueType::Expression:
                    throw Exceptions::SyntaxError(token, "Expressions are not assignable");

                /* process them together since they are essentially the same */
                case AST::Composite::ValueType::Array:
                case AST::Composite::ValueType::Tuple:
                {
                    /* restore the state and skip the start operator */
                    _lexer->popState();
                    _lexer->next();

                    /* process arrays and tuples accordingly */
                    if (comp->vtype == AST::Composite::ValueType::Array)
                        result->items.emplace_back(parseUnpack(Token::Operator::IndexRight));
                    else
                        result->items.emplace_back(parseUnpack(Token::Operator::BracketRight));

                    /* peek the next token */
                    token = _lexer->peek();
                    break;
                }
            }
        }

        /* check for the comma seperator */
        if (token->isOperator<Token::Operator::Comma>())
        {
            /* skip the comma */
            _lexer->next();
            token = _lexer->peek();

            /* may follows a terminator, if so, skip it and quit the loop */
            if ((token->is<Token::Type::Operators>()) && (token->asOperator() == terminator))
            {
                _lexer->next();
                break;
            }
        }

        /* check for the termination operator */
        else if ((token->is<Token::Type::Operators>()) && (token->asOperator() == terminator))
        {
            _lexer->next();
            break;
        }

        /* otherwise it's an error */
        else
            throw Exceptions::SyntaxError(token, Utils::Strings::format("Token %s expected", Token::toString(terminator)));
    }

    /* must have at least something */
    if (result->items.empty())
        throw Exceptions::SyntaxError(token);
    else
        return result;
}

std::unique_ptr<AST::Literal> Parser::parseLiteral(void)
{
    /* locate next token */
    Token::Ptr token = _lexer->next();
    Token::Type tokenType = token->type();

    /* check token type */
    switch (tokenType)
    {
        case Token::Type::String  : return std::make_unique<AST::Literal>(token, token->asString());
        case Token::Type::Decimal : return std::make_unique<AST::Literal>(token, token->asDecimal());
        case Token::Type::Integer : return std::make_unique<AST::Literal>(token, token->asInteger());

        case Token::Type::Eof:
        case Token::Type::Keywords:
        case Token::Type::Operators:
        case Token::Type::Identifiers:
            throw Exceptions::SyntaxError(token);
    }
}

std::unique_ptr<AST::Composite> Parser::parseComposite(CompositeSuggestion suggestion)
{
    /* locate next token */
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Composite> result;

    /* the root value */
    switch (token->type())
    {
        case Token::Type::Eof:
        case Token::Type::Keywords:
            throw Exceptions::SyntaxError(token);

        /* literal constants */
        case Token::Type::Decimal:
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
                    /* skip the '[' */
                    std::unique_ptr<AST::Array> array(new AST::Array(_lexer->next()));

                    /* iterate until meets `IndexRight` token */
                    while (!(_lexer->peek()->isOperator<Token::Operator::IndexRight>()))
                    {
                        /* parse an array item */
                        array->items.emplace_back(parseExpression());
                        token = _lexer->peek();

                        /* check for the comma seperator */
                        if (token->isOperator<Token::Operator::Comma>())
                            _lexer->next();

                        /* check for the `IndexRight` operator */
                        else if (token->isOperator<Token::Operator::IndexRight>())
                            break;

                        /* otherwise it's an error */
                        else
                            throw Exceptions::SyntaxError(token, "Operator \",\" or \"]\" expected");
                    }

                    /* make it an array */
                    _lexer->next();
                    result = std::make_unique<AST::Composite>(token, std::move(array));
                    break;
                }

                /* map expression */
                case Token::Operator::BlockLeft:
                {
                    /* skip the '{' */
                    std::unique_ptr<AST::Map> map(new AST::Map(_lexer->next()));
                    std::unique_ptr<AST::Expression> key;
                    std::unique_ptr<AST::Expression> value;

                    /* iterate until meets `BlockRight` token */
                    while (!(_lexer->peek()->isOperator<Token::Operator::BlockRight>()))
                    {
                        /* parse key and value */
                        key = parseExpression();
                        _lexer->operatorExpected<Token::Operator::Colon>();
                        value = parseExpression();

                        /* add to may key-value list */
                        map->items.emplace_back(std::make_pair(std::move(key), std::move(value)));
                        token = _lexer->peek();

                        /* check for the comma seperator */
                        if (token->isOperator<Token::Operator::Comma>())
                            _lexer->next();

                        /* check for the `IndexRight` operator */
                        else if (token->isOperator<Token::Operator::BlockRight>())
                            break;

                        /* otherwise it's an error */
                        else
                            throw Exceptions::SyntaxError(token, "Operator \",\" or \"}\" expected");
                    }

                    /* make it a map */
                    _lexer->next();
                    result = std::make_unique<AST::Composite>(token, std::move(map));
                    break;
                }

                /* tuple expression, or lambda function */
                case Token::Operator::BracketLeft:
                {
                    switch (suggestion)
                    {
                        /* parse as tuple */
                        case CompositeSuggestion::Tuple:
                        {
                            /* skip the '(' */
                            std::unique_ptr<AST::Tuple> tuple(new AST::Tuple(_lexer->next()));

                            /* iterate until meets `BracketRight` token */
                            while (!(_lexer->peek()->isOperator<Token::Operator::BracketRight>()))
                            {
                                /* parse an tuple item */
                                tuple->items.emplace_back(parseExpression());
                                token = _lexer->peek();

                                /* check for the comma seperator */
                                if (token->isOperator<Token::Operator::Comma>())
                                    _lexer->next();

                                /* check for the `BracketRight` operator */
                                else if (token->isOperator<Token::Operator::BracketRight>())
                                    break;

                                /* otherwise it's an error */
                                else
                                    throw Exceptions::SyntaxError(token, "Operator \",\" or \")\" expected");
                            }

                            /* make it a tuple */
                            _lexer->next();
                            result = std::make_unique<AST::Composite>(token, std::move(tuple));
                            break;
                        }

                        /* parse as lambda */
                        case CompositeSuggestion::Lambda:
                        {
                            result = std::make_unique<AST::Composite>(token, parseLambda());
                            break;
                        }

                        /* parse as a simple nested expression */
                        case CompositeSuggestion::Simple:
                        {
                            _lexer->next();
                            result = std::make_unique<AST::Composite>(token, parseExpression());
                            _lexer->operatorExpected<Token::Operator::BracketRight>();
                            break;
                        }
                    }

                    break;
                }

                /* other operators are not allowed */
                default:
                    throw Exceptions::SyntaxError(token);
            }

            break;
        }
    }

    /* may have modifiers */
    while ((token = _lexer->peekOrLine())->is<Token::Type::Operators>())
    {
        switch (token->asOperator())
        {
            /* attribute accessing */
            case Token::Operator::Point:
            {
                result->mods.emplace_back(parseAttribute());
                break;
            }

            /* function invoking */
            case Token::Operator::BracketLeft:
            {
                result->mods.emplace_back(parseInvoke());
                break;
            }

            /* indexing or slicing */
            case Token::Operator::IndexLeft:
            {
                /* save the state, and skip the "[" token */
                _lexer->pushState();
                _lexer->next();

                /* followed by a ":" operator, definately a slice */
                if (_lexer->peek()->isOperator<Token::Operator::Colon>())
                {
                    _lexer->popState();
                    result->mods.emplace_back(parseSlice());
                    break;
                }

                /* otherwise must followed by an expression */
                Token::Ptr next;
                std::unique_ptr<AST::Expression> expr = parseExpression();

                /* if the next operator is ":", definately a slice */
                if (_lexer->peek()->isOperator<Token::Operator::Colon>())
                {
                    _lexer->popState();
                    result->mods.emplace_back(parseSlice());
                    break;
                }

                /* otherwise it must be an index */
                if (!((next = _lexer->next())->isOperator<Token::Operator::IndexRight>()))
                    throw Exceptions::SyntaxError(next, "\"]\" or \":\" expected");

                /* preserve the state */
                _lexer->preserveState();
                std::unique_ptr<AST::Index> index(new AST::Index(token));

                /* and build the index */
                index->index = std::move(expr);
                result->mods.emplace_back(std::move(index));
                break;
            }

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

std::unique_ptr<AST::Expression> Parser::parseReturnExpression(void)
{
    /* try parse the first expression */
    Token::Ptr next;
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Tuple> tuple(new AST::Tuple(token));
    std::unique_ptr<AST::Expression> result = parseExpression();

    /* not followed by a comma, it's a simple expression */
    if (!(_lexer->peekOrLine()->isOperator<Token::Operator::Comma>()))
        return result;

    /* otherwise, it's a inline tuple */
    next = _lexer->nextOrLine();
    tuple->items.emplace_back(std::move(result));

    /* iterate until meets `NewLine` token */
    while (!(_lexer->peekOrLine()->isOperator<Token::Operator::NewLine>()))
    {
        /* parse an tuple item */
        tuple->items.emplace_back(parseExpression());
        next = _lexer->peekOrLine();

        /* check for the comma seperator */
        if (next->isOperator<Token::Operator::Comma>())
            _lexer->nextOrLine();

        /* check for the `NewLine` operator */
        else if (next->isOperator<Token::Operator::NewLine>())
            break;

        /* otherwise it's an error */
        else
            throw Exceptions::SyntaxError(next, "Operator \",\" or `NewLine` expected");
    }

    /* build a composite expression around it */
    return std::make_unique<AST::Expression>(token, std::make_unique<AST::Composite>(token, std::move(tuple)));
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
    /* prune the first operand, move the sub-expression up if possible */
    if (expr->first.type == AST::Expression::Operand::Type::Composite)
    {
        /* this composite has no modifiers, and contains just a simple expression, we can simply move it to the upper level */
        if (expr->first.composite->mods.empty() && (expr->first.composite->vtype == AST::Composite::ValueType::Expression))
        {
            expr->first.type = AST::Expression::Operand::Type::Expression;
            expr->first.expression = std::move(expr->first.composite->expression);
        }
    }

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
    {
        /* if the operand is a composite value, move the sub-expression up if possible */
        if (term.type == AST::Expression::Operand::Type::Composite)
        {
            /* this composite has no modifiers, and contains just a simple expression, we can simply move it to the upper level */
            if (term.composite->mods.empty() && (term.composite->vtype == AST::Composite::ValueType::Expression))
            {
                term.type = AST::Expression::Operand::Type::Expression;
                term.expression = std::move(term.composite->expression);
            }
        }

        /* then prune the "clean" sub-expression */
        if (term.type == AST::Expression::Operand::Type::Expression)
            pruneExpression(term.expression);
    }
}

void Parser::parseAssignTarget(std::unique_ptr<AST::Composite> &comp, std::unique_ptr<AST::Unpack> &unpack, Token::Operator terminator)
{
    /* save tokenizer state, and clear both nodes */
    comp.reset();
    unpack.reset();
    _lexer->pushState();

    /* parse the first element as expression */
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Expression> expr = parseFactor();

    /* check for following operator */
    if (_lexer->peek()->isOperator<Token::Operator::Comma>())
    {
        /* this is an inlined `Unpack` sequence, restore all the state and parse from start */
        _lexer->popState();
        unpack = std::move(parseUnpack(terminator));
    }
    else
    {
        /* must ends with a terminator token */
        if ((token = _lexer->next())->asOperator() != terminator)
            throw Exceptions::SyntaxError(token, Utils::Strings::format("Token %s expected", Token::toString(terminator)));

        /* should be a single `Composite`, so test and extract it */
        while (!comp)
        {
            /* has unary operators or following operands, it's an expression, and not assignable */
            if (expr->hasOp || !expr->follows.empty())
                throw Exceptions::SyntaxError(token, "Expressions are not assignable");

            /* if it's the composite, extract it, otherwise, move to inner expression */
            if (expr->first.type == AST::Expression::Operand::Type::Composite)
                comp = std::move(expr->first.composite);
            else
                expr = std::move(expr->first.expression);
        }

        /* it's a mutable composite, preserve the current tokenizer state and take it directly */
        if (comp->isSyntacticallyMutable(true))
        {
            _lexer->preserveState();
            return;
        }

        /* otherwise, it maybe a sub-sequence */
        switch (comp->vtype)
        {
            /* they are all immutable */
            case AST::Composite::ValueType::Map:
            case AST::Composite::ValueType::Name:
            case AST::Composite::ValueType::Literal:
            case AST::Composite::ValueType::Function:
            case AST::Composite::ValueType::Expression:
                throw Exceptions::SyntaxError(token, "Expressions are not assignable");

            /* process them together since they are essentially the same */
            case AST::Composite::ValueType::Array:
            case AST::Composite::ValueType::Tuple:
            {
                /* restore the state and skip the start operator */
                _lexer->popState();
                _lexer->next();

                /* process arrays and tuples accordingly */
                if (comp->vtype == AST::Composite::ValueType::Array)
                    unpack = std::move(parseUnpack(Token::Operator::IndexRight));
                else
                    unpack = std::move(parseUnpack(Token::Operator::BracketRight));

                /* clear composite node */
                comp.reset();
                break;
            }
        }
    }
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
        while ((token = _lexer->peek())->is<Token::Type::Operators>())                                  \
        {                                                                                               \
            /* check the operator */                                                                    \
            switch (token->asOperator())                                                                \
            {                                                                                           \
                /* encountered the required operators,                                                  \
                 * parse the following operands of the expression */                                    \
                MAKE_OPERATOR_LIST(__VA_ARGS__)                                                         \
                {                                                                                       \
                    _lexer->next();                                                                     \
                    result->follows.emplace_back(token, parse ## TermType());                           \
                    break;                                                                              \
                }                                                                                       \
                                                                                                        \
                /* not the operator we want */                                                          \
                default:                                                                                \
                    return result;                                                                      \
            }                                                                                           \
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
            throw Exceptions::SyntaxError(token);

        case Token::Type::Decimal:
        case Token::Type::String:
        case Token::Type::Integer:
        case Token::Type::Identifiers:
        {
            /* composite types (constants, names, arrays, maps, tuples, lambdas with attributes, indexes, and invokes) */
            return std::make_unique<AST::Expression>(token, parseComposite(CompositeSuggestion::Simple));
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
                    return std::make_unique<AST::Expression>(token, parseFactor(), true);
                }

                /* map or array expressions */
                case Token::Operator::BlockLeft:
                case Token::Operator::IndexLeft:
                {
                    /* composite types (constants, names, arrays, maps, tuples, lambdas with attributes, indexes, and invokes) */
                    return std::make_unique<AST::Expression>(token, parseComposite(CompositeSuggestion::Simple));
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
                        _lexer->next();
                        next = _lexer->peek();

                        /* perform a simple check to give the composite parser some suggestions */
                        if (!(next->isOperator<Token::Operator::Lambda>()))
                        {
                            _lexer->popState();
                            return std::make_unique<AST::Expression>(token, parseComposite(CompositeSuggestion::Tuple));
                        }
                        else
                        {
                            _lexer->popState();
                            return std::make_unique<AST::Expression>(token, parseComposite(CompositeSuggestion::Lambda));
                        }
                    }

                    do
                    {
                        /* '*' and '**' prefix operators encountered, definately a lambda function */
                        if (next->isOperator<Token::Operator::Power>() || next->isOperator<Token::Operator::Multiply>())
                        {
                            _lexer->popState();
                            return std::make_unique<AST::Expression>(token, parseComposite(CompositeSuggestion::Lambda));
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
                            throw Exceptions::SyntaxError(next, "Operator \",\" or \")\" expected");

                        /* once encountered a comma, the result may never be a nested expression */
                        next = _lexer->peek();
                        hasTail = true;

                    /* iterate until meets `BracketRight` token */
                    } while (!(next->isOperator<Token::Operator::BracketRight>()));

                    /* it has a tail comma, definately a tuple */
                    if (hasTail)
                    {
                        _lexer->popState();
                        return std::make_unique<AST::Expression>(token, parseComposite(CompositeSuggestion::Tuple));
                    }

                    /* skip the right bracket */
                    _lexer->next();
                    next = _lexer->peek();

                    /* check whether it maybe a lambda function, which must
                     * not have the tail comma, and all arguments must be names-only */
                    if (nameOnly && next->isOperator<Token::Operator::Lambda>())
                    {
                        _lexer->popState();
                        return std::make_unique<AST::Expression>(token, parseComposite(CompositeSuggestion::Lambda));
                    }

                    /* or there are more than one item, definately a tuple */
                    else if (n > 1)
                    {
                        _lexer->popState();
                        return std::make_unique<AST::Expression>(token, parseComposite(CompositeSuggestion::Tuple));
                    }

                    /* otherwise, it's just a simple nested expression */
                    else
                    {
                        _lexer->popState();
                        return std::make_unique<AST::Expression>(token, parseComposite(CompositeSuggestion::Simple));
                    }
                }

                default:
                    throw Exceptions::SyntaxError(token);
            }
        }
    }
}

/*** Generic Statements ***/

std::unique_ptr<AST::Statement> Parser::parseStatement(void)
{
    /* peek the first token */
    Token::Ptr token = _lexer->peek();
    std::unique_ptr<AST::Statement> result;

    /* compound statement, no semicolon allowed */
    if (token->isOperator<Token::Operator::BlockLeft>())
        return std::make_unique<AST::Statement>(token, parseCompondStatement());

    /* single semicolon, give an empty compound statement */
    else if (token->isOperator<Token::Operator::Semicolon>())
    {
        _lexer->next();
        return std::make_unique<AST::Statement>(token, std::make_unique<AST::CompoundStatement>(token));
    }

    /* decorator expression, parse as assigning invocation */
    else if (token->isOperator<Token::Operator::Decorator>())
    {
        Token::Ptr next = _lexer->next();
        std::unique_ptr<AST::Decorator> decorator(new AST::Decorator(next, parseExpression()));

        /* might be function or class */
        switch ((next = _lexer->peek())->asKeyword())
        {
            case Token::Keyword::Class:
            {
                decorator->klass = parseClass();
                decorator->decoration = AST::Decorator::Decoration::Class;
                break;
            }

            case Token::Keyword::Function:
            {
                decorator->function = parseFunction();
                decorator->decoration = AST::Decorator::Decoration::Function;
                break;
            }

            default:
                throw Exceptions::SyntaxError(next, "Can only decorate functions or classes");
        }

        /* build decorator expression statement */
        return std::make_unique<AST::Statement>(token, std::move(decorator));
    }

    /* keywords, such as "if", "for", "while" etc. */
    else if (token->is<Token::Type::Keywords>())
    {
        switch (token->asKeyword())
        {
            case Token::Keyword::If       : return std::make_unique<AST::Statement>(token, parseIf());
            case Token::Keyword::For      : return std::make_unique<AST::Statement>(token, parseFor());
            case Token::Keyword::Try      : return std::make_unique<AST::Statement>(token, parseTry());
            case Token::Keyword::Class    : return std::make_unique<AST::Statement>(token, parseClass());
            case Token::Keyword::While    : return std::make_unique<AST::Statement>(token, parseWhile());
            case Token::Keyword::Native   : return std::make_unique<AST::Statement>(token, parseNative());
            case Token::Keyword::Switch   : return std::make_unique<AST::Statement>(token, parseSwitch());
            case Token::Keyword::Function : return std::make_unique<AST::Statement>(token, parseFunction());

            case Token::Keyword::Break    : result = std::make_unique<AST::Statement>(token, parseBreak()); break;
            case Token::Keyword::Return   : result = std::make_unique<AST::Statement>(token, parseReturn()); break;
            case Token::Keyword::Continue : result = std::make_unique<AST::Statement>(token, parseContinue()); break;

            case Token::Keyword::Raise    : result = std::make_unique<AST::Statement>(token, parseRaise()); break;
            case Token::Keyword::Delete   : result = std::make_unique<AST::Statement>(token, parseDelete()); break;
            case Token::Keyword::Import   : result = std::make_unique<AST::Statement>(token, parseImport()); break;

            default:
                throw Exceptions::SyntaxError(token);
        }
    }

    /* assignment or incremental */
    else
    {
        /* save the tokenizer state */
        _lexer->pushState();

        /* try parsing the first part as incremental statement */
        Token::Ptr next = _lexer->peek();
        std::unique_ptr<AST::Expression> expr = parseExpression();
        std::unique_ptr<AST::Incremental> incr(new AST::Incremental(next));

        /* must be operators */
        switch ((incr->op = _lexer->nextOrLine())->asOperator())
        {
            /* a solo expression */
            case Token::Operator::NewLine:
            case Token::Operator::Semicolon:
            {
                _lexer->preserveState();
                return std::make_unique<AST::Statement>(token, std::move(expr));
            }

            /* incremental statements */
            case Token::Operator::InplaceAdd:
            case Token::Operator::InplaceSub:
            case Token::Operator::InplaceMul:
            case Token::Operator::InplaceDiv:
            case Token::Operator::InplaceMod:
            case Token::Operator::InplacePower:
            case Token::Operator::InplaceBitOr:
            case Token::Operator::InplaceBitAnd:
            case Token::Operator::InplaceBitXor:
            case Token::Operator::InplaceShiftLeft:
            case Token::Operator::InplaceShiftRight:
            {
                /* is a valid incremental format, preserve the tokenizer state */
                _lexer->preserveState();

                /* should be a single `Composite`, so test and extract it */
                while (!(incr->dest))
                {
                    /* has unary operators or following operands, it's an expression, and not assignable */
                    if (expr->hasOp || !expr->follows.empty())
                        throw Exceptions::SyntaxError(token, "Expressions are not assignable");

                    /* if it's the composite, extract it, otherwise, move to inner expression */
                    if (expr->first.type != AST::Expression::Operand::Type::Composite)
                        expr = std::move(expr->first.expression);
                    else
                        incr->dest = std::move(expr->first.composite);
                }

                /* must be mutable, and not slicing */
                if (!(incr->dest->isSyntacticallyMutable(false)))
                    throw Exceptions::SyntaxError(token, "Expressions are not assignable");

                /* parse the expression */
                incr->expr = parseExpression();
                result = std::make_unique<AST::Statement>(token, std::move(incr));
                break;
            }

            /* other operators, maybe an assignment statement */
            default:
            {
                /* restore the tokenizer state */
                _lexer->popState();
                std::unique_ptr<AST::Assign> assign(new AST::Assign(token));

                /* <sequence> = <return-expr> */
                parseAssignTarget(assign->composite, assign->unpack, Token::Operator::Assign);
                assign->expression = parseReturnExpression();
                result = std::make_unique<AST::Statement>(token, std::move(assign));
            }
        }
    }

    /* a statement must ends with semicolon or a new line, or EOF */
    if (!((token = _lexer->nextOrLine())->is<Token::Type::Eof>()))
    {
        switch (token->asOperator())
        {
            case Token::Operator::NewLine:
            case Token::Operator::Semicolon:
                break;

            default:
                throw Exceptions::SyntaxError(token, "New line or semicolon expected");
        }
    }

    return result;
}

std::unique_ptr<AST::CompoundStatement> Parser::parseCompondStatement(void)
{
    Token::Ptr token = _lexer->next();
    std::unique_ptr<AST::CompoundStatement> result(new AST::CompoundStatement(token));

    /* block start */
    if (!(token->isOperator<Token::Operator::BlockLeft>()))
        throw Exceptions::SyntaxError(token, "Operator \"{\" expected");

    /* parse each statement */
    while (!(_lexer->peek()->isOperator<Token::Operator::BlockRight>()))
        result->statements.emplace_back(parseStatement());

    /* skip block end */
    _lexer->next();
    return result;
}
}
