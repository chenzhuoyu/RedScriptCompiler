#ifndef REDSCRIPT_COMPILER_AST_H
#define REDSCRIPT_COMPILER_AST_H

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "compiler/Tokenizer.h"

namespace RedScript::Compiler::AST
{
/*** Basic AST Nodes ***/

class Node
{
public:
    enum class Type : int
    {
        If,
        For,
        While,
        Foreach,

        Index,
        Invoke,
        Attribute,

        Name,
        Unpack,
        Literal,
        Composite,
        Expression,

        Statement,
        CompondStatement,
    };

private:
    int _row;
    int _col;
    Type _type;

public:
    virtual ~Node() {}
    explicit Node(Type type, const Token::Ptr &token) : _row(token->row()), _col(token->col()), _type(type) {}

public:
    int row(void) const { return _row; }
    int col(void) const { return _col; }
    Type type(void) const { return _type; }

};

/* AST Node Common Constructor */
#define AST_NODE(NodeName)  \
    explicit NodeName(const Token::Ptr &token) : Node(Node::Type::NodeName, token) {}

struct If;
struct For;
struct While;
struct Foreach;

struct Index;
struct Invoke;
struct Attribute;

struct Name;
struct Unpack;
struct Literal;
struct Composite;
struct Expression;

struct Statement;
struct CompondStatement;

/*** Control Flows ***/

struct If : public Node
{
    AST_NODE(If)
    std::unique_ptr<Expression> expr;
    std::unique_ptr<Statement > positive;
    std::unique_ptr<Statement > negative;
};

struct For : public Node
{
    AST_NODE(For)
    std::unique_ptr<Statement > init;
    std::unique_ptr<Expression> expr;
    std::unique_ptr<Statement > step;
    std::unique_ptr<Statement > body;
};

struct While : public Node
{
    AST_NODE(While)
    std::unique_ptr<Expression> expr;
    std::unique_ptr<Statement > body;
};

struct Foreach : public Node
{
    AST_NODE(Foreach)
    std::unique_ptr<Name      > name;
    std::unique_ptr<Unpack    > pack;
    std::unique_ptr<Expression> expr;
    std::unique_ptr<Statement > body;
};

/*** Object Modifiers ***/

struct Index : public Node
{
    AST_NODE(Index);
};

struct Invoke : public Node
{
    AST_NODE(Invoke)
};

struct Attribute : public Node
{
    AST_NODE(Attribute)
};

/*** Expressions ***/

struct Name : public Node
{
    AST_NODE(Name)
    std::string name;
};

struct Unpack : public Node
{
    struct Target
    {
        enum class Type : int
        {
            Name,
            Subset,
        };

    public:
        Type type;
        std::unique_ptr<AST::Name> name = nullptr;
        std::unique_ptr<AST::Unpack> subset = nullptr;

    public:
        Target(std::unique_ptr<AST::Name> &&name) : type(Type::Name), name(std::move(name)) {}
        Target(std::unique_ptr<AST::Unpack> &&subset) : type(Type::Subset), subset(std::move(subset)) {}

    };

public:
    AST_NODE(Unpack)
    std::vector<Target> items;

};

struct Literal : public Node
{
    enum class Type : int
    {
        String,
        Decimal,
        Integer,
    };

public:
    Type type;
    double decimal = 0.0;
    int64_t integer = 0;
    std::string string = "";

public:
    explicit Literal(const Token::Ptr &token, double value) : Node(Node::Type::Literal, token), type(Type::Decimal), decimal(value) {}
    explicit Literal(const Token::Ptr &token, int64_t value) : Node(Node::Type::Literal, token), type(Type::Integer), integer(value) {}
    explicit Literal(const Token::Ptr &token, std::string &&value) : Node(Node::Type::Literal, token), type(Type::String), string(std::move(value)) {}

};

struct Composite : public Node
{
    AST_NODE(Composite)

public:
    enum class ModType : int
    {
        Index,
        Invoke,
        Attribute,
    };

public:
    enum class ValueType : int
    {
        Name,
        Literal,
    };

public:
    struct Modifier
    {
        ModType type;
        std::unique_ptr<AST::Index> index;
        std::unique_ptr<AST::Invoke> invoke;
        std::unique_ptr<AST::Attribute> attribute;

    public:
        Modifier(std::unique_ptr<AST::Index> &&value) : type(ModType::Index), index(std::move(value)) {}
        Modifier(std::unique_ptr<AST::Invoke> &&value) : type(ModType::Invoke), invoke(std::move(value)) {}
        Modifier(std::unique_ptr<AST::Attribute> &&value) : type(ModType::Attribute), attribute(std::move(value)) {}

    };

public:
    ValueType vtype;
    std::vector<Modifier> mods;
    std::unique_ptr<AST::Name> name = nullptr;
    std::unique_ptr<AST::Literal> literal = nullptr;

public:
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Name> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Name), name(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Literal> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Literal), literal(std::move(value)) {}

};

struct Expression : public Node
{
    AST_NODE(Expression);

public:
    struct Operand
    {
        enum class Type : int
        {
            Composite,
            Expression,
        };

    public:
        Type type;
        std::unique_ptr<AST::Composite> composite = nullptr;
        std::unique_ptr<AST::Expression> expression = nullptr;

    public:
        Operand(std::unique_ptr<AST::Composite> &&value) : type(Type::Composite), composite(std::move(value)) {}
        Operand(std::unique_ptr<AST::Expression> &&value) : type(Type::Expression), expression(std::move(value)) {}

    };

public:
    Token::Operator op;
    std::unique_ptr<Operand> left = nullptr;
    std::unique_ptr<Expression> right = nullptr;

public:
    explicit Expression(const Token::Ptr &token, std::unique_ptr<AST::Composite> &&left) : Node(Node::Type::Expression, token), left(new Operand(std::move(left))) {}
    explicit Expression(const Token::Ptr &token, std::unique_ptr<AST::Expression> &&left) : Node(Node::Type::Expression, token), left(new Operand(std::move(left))) {}
    explicit Expression(const Token::Ptr &token, std::unique_ptr<AST::Expression> &&right, Token::Operator op) : Node(Node::Type::Expression, token), right(std::move(right)), op(op) {}

};

/*** Generic Statements ***/

struct Statement : public Node
{
    AST_NODE(Statement)
    std::unique_ptr<Node> statement;
};

struct CompondStatement : public Node
{
    AST_NODE(CompondStatement)
    std::vector<std::unique_ptr<Statement>> statements;
};

/* end of #define AST_NODE(NodeName) ... */
#undef AST_NODE
}

#endif /* REDSCRIPT_COMPILER_AST_H */
