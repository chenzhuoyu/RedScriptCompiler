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
        Function,

        Index,
        Invoke,
        Attribute,

        Map,
        Array,
        Tuple,

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
struct Function;

struct Index;
struct Invoke;
struct Attribute;

struct Map;
struct Array;
struct Tuple;

struct Name;
struct Unpack;
struct Literal;
struct Composite;
struct Expression;

struct Statement;
struct CompondStatement;

/*** Basic Language Structures ***/

struct If : public Node
{
    AST_NODE(If)
    std::unique_ptr<Expression> expr;
    std::unique_ptr<Statement > positive;
    std::unique_ptr<Statement > negative = nullptr;
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

struct Function : public Node
{
    AST_NODE(Function)

public:
    std::unique_ptr<Name> name = nullptr;
    std::unique_ptr<Name> vargs = nullptr;
    std::unique_ptr<Name> kwargs = nullptr;

public:
    std::unique_ptr<Statement> body;
    std::vector<std::unique_ptr<Name>> args;

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

/*** Composite Literals ***/

struct Map : public Node
{
    AST_NODE(Map)
    std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> items;
};

struct Array : public Node
{
    AST_NODE(Array)
    std::vector<std::unique_ptr<Expression>> items;
};

struct Tuple : public Node
{
    AST_NODE(Tuple)
    std::vector<std::unique_ptr<Expression>> items;
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
            Subset,
            Composite,
        };

    public:
        Type type;
        std::unique_ptr<AST::Unpack> subset = nullptr;
        std::unique_ptr<AST::Composite> composite = nullptr;

    public:
        Target(std::unique_ptr<AST::Unpack> &&value) : type(Type::Subset), subset(std::move(value)) {}
        Target(std::unique_ptr<AST::Composite> &&value) : type(Type::Composite), composite(std::move(value)) {}

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
        Map,
        Name,
        Array,
        Tuple,
        Literal,
        Function,
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

public:
    std::unique_ptr<AST::Map> map = nullptr;
    std::unique_ptr<AST::Name> name = nullptr;
    std::unique_ptr<AST::Array> array = nullptr;
    std::unique_ptr<AST::Tuple> tuple = nullptr;
    std::unique_ptr<AST::Literal> literal = nullptr;
    std::unique_ptr<AST::Function> function = nullptr;

public:
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Map> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Map), map(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Name> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Name), name(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Array> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Array), array(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Tuple> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Tuple), tuple(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Literal> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Literal), literal(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Function> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Function), function(std::move(value)) {}

public:
    bool isSyntacticallyMutable(void) const
    {
        if (mods.empty())
            return vtype == ValueType::Name;
        else
            return mods.back().type != ModType::Invoke;
    }
};

struct Expression : public Node
{
    struct Operand
    {
        enum class Type : int
        {
            Composite,
            Expression,
        };

    public:
        Type type;
        Token::Operator op;

    public:
        std::unique_ptr<AST::Composite> composite = nullptr;
        std::unique_ptr<AST::Expression> expression = nullptr;

    public:
        Operand(std::unique_ptr<AST::Composite> &&value) : type(Type::Composite), composite(std::move(value)) {}
        Operand(std::unique_ptr<AST::Expression> &&value) : type(Type::Expression), expression(std::move(value)) {}

    public:
        /* special operator to create operands with operators (e.g. unary expression or following operands) */
        Operand(Token::Operator op, std::unique_ptr<AST::Expression> &&value) : op(op), type(Type::Expression), expression(std::move(value)) {}

    };

public:
    bool hasOp;
    Operand first;
    std::vector<Operand> follows;

public:
    explicit Expression(const Token::Ptr &token, std::unique_ptr<AST::Composite> &&value) : Node(Node::Type::Expression, token), first(std::move(value)), hasOp(false) {}
    explicit Expression(const Token::Ptr &token, std::unique_ptr<AST::Expression> &&value) : Node(Node::Type::Expression, token), first(std::move(value)), hasOp(false) {}

public:
    /* special operator to create unary operator expressions */
    explicit Expression(const Token::Ptr &token, std::unique_ptr<AST::Expression> &&value, Token::Operator op) : Node(Node::Type::Expression, token), first(op, std::move(value)), hasOp(true) {}

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
