#ifndef REDSCRIPT_COMPILER_TOKENIZER_H
#define REDSCRIPT_COMPILER_TOKENIZER_H

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "utils/Decimal.h"
#include "utils/Strings.h"
#include "exceptions/SyntaxError.h"

namespace RedScript::Compiler
{
class Token
{
    int _row;
    int _col;

public:
    enum class Type : int
    {
        Eof,
        String,
        Decimal,
        Integer,
        Keywords,
        Operators,
        Identifiers,
    };

    enum class Keyword : int
    {
        If,
        Else,
        For,
        While,
        Switch,
        Case,
        Default,

        Break,
        Continue,
        Return,

        Try,
        Except,
        Finally,
        Raise,

        Class,
        Native,
        Function,

        As,
        From,
        Delete,
        Import,
    };

    enum class Operator : int
    {
        BracketLeft,
        BracketRight,
        IndexLeft,
        IndexRight,
        BlockLeft,
        BlockRight,

        Comma,
        Point,
        Colon,
        Semicolon,
        NewLine,

        Less,
        Greater,
        Leq,
        Geq,
        Equ,
        Neq,

        BoolAnd,
        BoolOr,
        BoolNot,

        Plus,
        Minus,
        Divide,
        Multiply,
        Module,
        Power,

        BitAnd,
        BitOr,
        BitNot,
        BitXor,
        ShiftLeft,
        ShiftRight,

        InplaceAdd,
        InplaceSub,
        InplaceMul,
        InplaceDiv,
        InplaceMod,
        InplacePower,

        InplaceBitAnd,
        InplaceBitOr,
        InplaceBitXor,
        InplaceShiftLeft,
        InplaceShiftRight,

        In,
        Assign,
        Lambda,
        Decorator,
    };

public:
    typedef std::shared_ptr<Token> Ptr;

private:
    Type _type;

private:
    int64_t _integer = 0;
    std::string _string = "";
    Utils::Decimal _decimal = {};

private:
    Keyword _keyword;
    Operator _operator;

private:
    explicit Token(int row, int col) : _row(row), _col(col), _type(Type::Eof) {}
    explicit Token(int row, int col, Type type, const std::string &value) : _row(row), _col(col), _type(type), _string(value) {}

private:
    explicit Token(int row, int col, int64_t value) : _row(row), _col(col), _type(Type::Integer), _integer(value) {}
    explicit Token(int row, int col, const Utils::Decimal &value) : _row(row), _col(col), _type(Type::Decimal), _decimal(value) {}

private:
    explicit Token(int row, int col, Keyword value) : _row(row), _col(col), _type(Type::Keywords), _keyword(value) {}
    explicit Token(int row, int col, Operator value) : _row(row), _col(col), _type(Type::Operators), _operator(value) {}

public:
    int row(void) const { return _row; }
    int col(void) const { return _col; }
    Type type(void) const { return _type; }

public:
    template <Type T> bool is(void) const { return _type == T; }
    template <Keyword Kw> bool isKeyword(void) const { return is<Type::Keywords>() && (_keyword == Kw); }
    template <Operator Op> bool isOperator(void) const { return is<Type::Operators>() && (_operator == Op); }

public:
    void asEof(void) const
    {
        if (_type != Type::Eof)
            throw Exceptions::SyntaxError(this, Utils::Strings::format("\"Eof\" expected, but got \"%s\"", toString()));
    }

public:
    int64_t asInteger(void) const
    {
        if (_type == Type::Integer)
            return _integer;
        else
            throw Exceptions::SyntaxError(this, Utils::Strings::format("\"Integer\" expected, but got \"%s\"", toString()));
    }

public:
    Utils::Decimal asDecimal(void) const
    {
        if (_type == Type::Decimal)
            return _decimal;
        else
            throw Exceptions::SyntaxError(this, Utils::Strings::format("\"Decimal\" expected, but got \"%s\"", toString()));
    }

public:
    Keyword asKeyword (void) const
    {
        if (_type == Type::Keywords)
            return _keyword;
        else
            throw Exceptions::SyntaxError(this, Utils::Strings::format("\"Keyword\" expected, but got \"%s\"", toString()));
    }

public:
    Operator asOperator(void) const
    {
        if (_type == Type::Operators)
            return _operator;
        else
            throw Exceptions::SyntaxError(this, Utils::Strings::format("\"Operator\" expected, but got \"%s\"", toString()));
    }

public:
    std::string asString(void)
    {
        if (_type == Type::String)
            return _string;
        else
            throw Exceptions::SyntaxError(this, Utils::Strings::format("\"String\" expected, but got \"%s\"", toString()));
    }

public:
    std::string asIdentifier(void)
    {
        if (_type == Type::Identifiers)
            return _string;
        else
            throw Exceptions::SyntaxError(this, Utils::Strings::format("\"Identifier\" expected, but got \"%s\"", toString()));
    }

public:
    std::string toString(void) const
    {
        switch (_type)
        {
            case Type::Eof          : return "<Eof>";
            case Type::String       : return Utils::Strings::format("<String %s>"     , Utils::Strings::repr(_string.data(), _string.size()));
            case Type::Decimal      : return Utils::Strings::format("<Decimal %s>"    , _decimal.toString());
            case Type::Integer      : return Utils::Strings::format("<Integer %ld>"   , _integer);
            case Type::Identifiers  : return Utils::Strings::format("<Identifier %s>" , _string);

            case Type::Keywords     : return Token::toString(_keyword);
            case Type::Operators    : return Token::toString(_operator);
        }
    }

public:
    static constexpr const char *toString(Keyword value)
    {
        switch (value)
        {
            case Keyword::If        : return "<Keyword if>";
            case Keyword::Else      : return "<Keyword else>";
            case Keyword::For       : return "<Keyword for>";
            case Keyword::While     : return "<Keyword while>";
            case Keyword::Switch    : return "<Keyword switch>";
            case Keyword::Case      : return "<Keyword case>";
            case Keyword::Default   : return "<Keyword default>";

            case Keyword::Break     : return "<Keyword break>";
            case Keyword::Continue  : return "<Keyword continue>";
            case Keyword::Return    : return "<Keyword return>";

            case Keyword::Try       : return "<Keyword try>";
            case Keyword::Except    : return "<Keyword except>";
            case Keyword::Finally   : return "<Keyword finally>";
            case Keyword::Raise     : return "<Keyword raise>";

            case Keyword::Class     : return "<Keyword class>";
            case Keyword::Native    : return "<Keyword native>";
            case Keyword::Function  : return "<Keyword def>";

            case Keyword::As        : return "<Keyword as>";
            case Keyword::From      : return "<Keyword from>";
            case Keyword::Delete    : return "<Keyword delete>";
            case Keyword::Import    : return "<Keyword import>";
        }
    }

public:
    static constexpr const char *toString(Operator value)
    {
        switch (value)
        {
            case Operator::BracketLeft          : return "<Operator '('>";
            case Operator::BracketRight         : return "<Operator ')'>";
            case Operator::IndexLeft            : return "<Operator '['>";
            case Operator::IndexRight           : return "<Operator ']'>";
            case Operator::BlockLeft            : return "<Operator '{'>";
            case Operator::BlockRight           : return "<Operator '}'>";

            case Operator::Comma                : return "<Operator ','>";
            case Operator::Point                : return "<Operator '.'>";
            case Operator::Colon                : return "<Operator ':'>";
            case Operator::Semicolon            : return "<Operator ';'>";
            case Operator::NewLine              : return "<Operator NewLine>";

            case Operator::Less                 : return "<Operator '<'>";
            case Operator::Greater              : return "<Operator '>'>";
            case Operator::Leq                  : return "<Operator '<='>";
            case Operator::Geq                  : return "<Operator '>='>";
            case Operator::Equ                  : return "<Operator '=='>";
            case Operator::Neq                  : return "<Operator '!='>";

            case Operator::BoolAnd              : return "<Operator 'and'>";
            case Operator::BoolOr               : return "<Operator 'or'>";
            case Operator::BoolNot              : return "<Operator 'not'>";

            case Operator::Plus                 : return "<Operator '+'>";
            case Operator::Minus                : return "<Operator '-'>";
            case Operator::Divide               : return "<Operator '/'>";
            case Operator::Multiply             : return "<Operator '*'>";
            case Operator::Module               : return "<Operator '%'>";
            case Operator::Power                : return "<Operator '**'>";

            case Operator::BitAnd               : return "<Operator '&'>";
            case Operator::BitOr                : return "<Operator '|'>";
            case Operator::BitNot               : return "<Operator '~'>";
            case Operator::BitXor               : return "<Operator '^'>";
            case Operator::ShiftLeft            : return "<Operator '<<'>";
            case Operator::ShiftRight           : return "<Operator '>>'>";

            case Operator::InplaceAdd           : return "<Operator '+='>";
            case Operator::InplaceSub           : return "<Operator '-='>";
            case Operator::InplaceMul           : return "<Operator '*='>";
            case Operator::InplaceDiv           : return "<Operator '/='>";
            case Operator::InplaceMod           : return "<Operator '%='>";
            case Operator::InplacePower         : return "<Operator '**='>";

            case Operator::InplaceBitAnd        : return "<Operator '&='>";
            case Operator::InplaceBitOr         : return "<Operator '|='>";
            case Operator::InplaceBitXor        : return "<Operator '^='>";
            case Operator::InplaceShiftLeft     : return "<Operator '<<='>";
            case Operator::InplaceShiftRight    : return "<Operator '>>='>";

            case Operator::In                   : return "<Operator 'in'>";
            case Operator::Assign               : return "<Operator '='>";
            case Operator::Lambda               : return "<Operator '->'>";
            case Operator::Decorator            : return "<Operator '@'>";
        }
    }

public:
    static inline Ptr createEof(int row, int col) { return Ptr(new Token(row, col)); }

public:
    static inline Ptr createValue(int row, int col, int64_t value) { return Ptr(new Token(row, col, value)); }
    static inline Ptr createValue(int row, int col, Utils::Decimal value) { return Ptr(new Token(row, col, value)); }

public:
    static inline Ptr createKeyword(int row, int col, Keyword value) { return Ptr(new Token(row, col, value)); }
    static inline Ptr createOperator(int row, int col, Operator value) { return Ptr(new Token(row, col, value)); }

public:
    static inline Ptr createString(int row, int col, const std::string &value) { return Ptr(new Token(row, col, Type::String, value)); }
    static inline Ptr createIdentifier(int row, int col, const std::string &value) { return Ptr(new Token(row, col, Type::Identifiers, value)); }

};

class Tokenizer
{
    struct State
    {
        int row;
        int col;
        int pos;
        std::deque<Token::Ptr> cache;
    };

private:
    State *_state;
    std::string _source;
    std::vector<State> _stack;

public:
    explicit Tokenizer(const std::string &source);

public:
    char peekChar(bool isRaw = false);
    char nextChar(bool isRaw = false);

private:
    void skipSpaces(void);
    void skipComments(void);

private:
    Token::Ptr read(void);
    Token::Ptr readString(void);
    Token::Ptr readNumber(void);
    Token::Ptr readOperator(void);
    Token::Ptr readIdentifier(void);

public:
    int row(void) const { return _state->row; }
    int col(void) const { return _state->col; }
    int pos(void) const { return _state->pos; }

public:
    void popState(void)
    {
        _stack.pop_back();
        _state = &(_stack.back());
    }

public:
    void pushState(void)
    {
        _stack.push_back(State(_stack.back()));
        _state = &(_stack.back());
    }

public:
    void preserveState(void)
    {
        _stack.erase(----_stack.end());
        _state = &(_stack.back());
    }

public:
    Token::Ptr next(void);
    Token::Ptr peek(void);

public:
    Token::Ptr nextOrLine(void);
    Token::Ptr peekOrLine(void);

public:
    template <Token::Keyword value>
    void keywordExpected(void)
    {
        /* fetch next token */
        Token::Ptr token = next();

        /* check for token type and value */
        if (!token->is<Token::Type::Keywords>() || (token->asKeyword() != value))
            throw Exceptions::SyntaxError(token, Utils::Strings::format("%s expected", Token::toString(value)));
    }

public:
    template <Token::Operator value>
    void operatorExpected(void)
    {
        /* fetch next token */
        Token::Ptr token = next();

        /* check for token type and value */
        if (!token->is<Token::Type::Operators>() || (token->asOperator() != value))
            throw Exceptions::SyntaxError(token, Utils::Strings::format("%s expected", Token::toString(value)));
    }
};
}

#endif /* REDSCRIPT_COMPILER_TOKENIZER_H */
