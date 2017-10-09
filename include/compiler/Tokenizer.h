#ifndef REDSCRIPT_COMPILER_TOKENIZER_H
#define REDSCRIPT_COMPILER_TOKENIZER_H

#include <stack>
#include <memory>
#include <string>

#include "utils/Strings.h"
#include "runtime/SyntaxError.h"

namespace RedScript::Compiler
{
class Token
{
    int _row;
    int _col;

public:
    enum Type
    {
        Eof,
        Float,
        String,
        Integer,
        Keywords,
        Operators,
        Identifiers,
    };

    enum Keyword
    {
        If,
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
        Function,

        As,
        In,
        Delete,
        Import,
    };

    enum Operator
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
        BoolXor,

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

        Assign,
        Tuple,
        Range,
        Decorator,
    };

public:
    typedef std::shared_ptr<Token> Ptr;

private:
    Type _type;

private:
    double _float = 0.0;
    int64_t _integer = 0;
    std::string _string = "";

private:
    Keyword _keyword;
    Operator _operator;

private:
    explicit Token(int row, int col) : _row(row), _col(col), _type(Eof) {}
    explicit Token(int row, int col, Type type, const std::string &value) : _row(row), _col(col), _type(type), _string(value) {}

private:
    explicit Token(int row, int col, double value) : _row(row), _col(col), _type(Float), _float(value) {}
    explicit Token(int row, int col, int64_t value) : _row(row), _col(col), _type(Integer), _integer(value) {}

private:
    explicit Token(int row, int col, Keyword value) : _row(row), _col(col), _type(Keywords), _keyword(value) {}
    explicit Token(int row, int col, Operator value) : _row(row), _col(col), _type(Operators), _operator(value) {}

public:
    int row(void) const { return _row; }
    int col(void) const { return _col; }
    Type type(void) const { return _type; }

public:
    template <Type T>
    bool is(void) const { return _type == T; }

public:
    double  asFloat  (void) const { if (_type == Float  ) return _float  ; else throw Runtime::SyntaxError(_row, _col, Utils::Strings::format("\"Float\" expected, but got \"%s\""  , toString())); }
    int64_t asInteger(void) const { if (_type == Integer) return _integer; else throw Runtime::SyntaxError(_row, _col, Utils::Strings::format("\"Integer\" expected, but got \"%s\"", toString())); }

public:
    Keyword  asKeyword (void) const { if (_type == Keywords ) return _keyword ; else throw Runtime::SyntaxError(_row, _col, Utils::Strings::format("\"Keyword\" expected, but got \"%s\""   , toString())); }
    Operator asOperator(void) const { if (_type == Operators) return _operator; else throw Runtime::SyntaxError(_row, _col, Utils::Strings::format("\"Operator\" expected, but got \"%s\""  , toString())); }

public:
    const std::string &asString    (void) const { if (_type == String    ) return _string; else throw Runtime::SyntaxError(_row, _col, Utils::Strings::format("\"String\" expected, but got \"%s\""    , toString())); }
    const std::string &asIdentifier(void) const { if (_type == Identifiers) return _string; else throw Runtime::SyntaxError(_row, _col, Utils::Strings::format("\"Identifier\" expected, but got \"%s\"", toString())); }

public:
    void asEof(void) const
    {
        if (_type != Eof)
            throw Runtime::SyntaxError(_row, _col, Utils::Strings::format("\"Eof\" expected, but got \"%s\"", toString()));
    }

public:
    std::string toString(void) const;

public:
    static inline Ptr createEof(int row, int col) { return Ptr(new Token(row, col)); }

public:
    static inline Ptr createValue(int row, int col, double value) { return Ptr(new Token(row, col, value)); }
    static inline Ptr createValue(int row, int col, int64_t value) { return Ptr(new Token(row, col, value)); }

public:
    static inline Ptr createKeyword(int row, int col, Keyword value) { return Ptr(new Token(row, col, value)); }
    static inline Ptr createOperator(int row, int col, Operator value) { return Ptr(new Token(row, col, value)); }

public:
    static inline Ptr createString(int row, int col, const std::string &value) { return Ptr(new Token(row, col, String, value)); }
    static inline Ptr createIdentifier(int row, int col, const std::string &value) { return Ptr(new Token(row, col, Identifiers, value)); }

};

class Tokenizer
{
    struct State
    {
        int row;
        int col;
        int pos;
        Token::Ptr cache;
    };

private:
    State *_state;
    std::string _source;
    std::stack<State> _stack;

public:
    explicit Tokenizer(const std::string &source);

private:
    char peekChar(void);
    char nextChar(void);

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
    void popState(void)
    {
        _stack.pop();
        _state = &(_stack.top());
    }

public:
    void pushState(void)
    {
        _stack.push((const State &) _stack.top());
        _state = &(_stack.top());
    }

public:
    Token::Ptr next(void);
    Token::Ptr peek(void);

public:
    Token::Ptr nextOrLine(void);
    Token::Ptr peekOrLine(void);

};
}

#endif /* REDSCRIPT_COMPILER_TOKENIZER_H */
