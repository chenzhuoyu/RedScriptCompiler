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
        Class,
        While,
        Switch,
        Foreach,
        Function,

        Assign,
        Incremental,

        Break,
        Return,
        Continue,

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
struct Class;
struct While;
struct Switch;
struct Foreach;
struct Function;

struct Assign;
struct Incremental;

struct Break;
struct Return;
struct Continue;

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

struct Class : public Node
{
    AST_NODE(Class)
};

struct While : public Node
{
    AST_NODE(While)
    std::unique_ptr<Expression> expr;
    std::unique_ptr<Statement > body;
};

struct Switch : public Node
{
    AST_NODE(Switch)
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

struct Assign : public Node
{
    AST_NODE(Assign);

public:
    enum class Type : int
    {
        Unpack,
        Composite,
    };

public:
    Type vtype;
    std::unique_ptr<AST::Unpack> unpack;
    std::unique_ptr<AST::Composite> composite;
    std::unique_ptr<AST::Expression> expression;

};

struct Incremental : public Node
{
    AST_NODE(Incremental)
    Token::Operator op;
    std::unique_ptr<Composite> dest;
    std::unique_ptr<Expression> expr;
};

/*** Control Flows ***/

struct Break : public Node
{
    AST_NODE(Break)
};

struct Return : public Node
{
    AST_NODE(Return)
    std::unique_ptr<Expression> value;

public:
    explicit Return(const Token::Ptr &token, std::unique_ptr<Expression> &&value) : Node(Node::Type::Return, token), value(std::move(value)) {}

};

struct Continue : public Node
{
    AST_NODE(Continue)
};

/*** Object Modifiers ***/

struct Index : public Node
{
    AST_NODE(Index);
    std::unique_ptr<Expression> index;
};

struct Invoke : public Node
{
    AST_NODE(Invoke)
    bool discardResult = false;
    std::unique_ptr<Expression> varg = nullptr;
    std::unique_ptr<Expression> kwarg = nullptr;
    std::vector<std::unique_ptr<Expression>> args;
    std::vector<std::pair<std::unique_ptr<Name>, std::unique_ptr<Expression>>> kwargs;
};

struct Attribute : public Node
{
    AST_NODE(Attribute)
    std::unique_ptr<Name> attr;
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
        Expression,
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
    std::unique_ptr<AST::Expression> expression = nullptr;

public:
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Map> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Map), map(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Name> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Name), name(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Array> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Array), array(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Tuple> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Tuple), tuple(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Literal> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Literal), literal(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Function> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Function), function(std::move(value)) {}
    explicit Composite(const Token::Ptr &token, std::unique_ptr<AST::Expression> &&value) : Node(Node::Type::Composite, token), vtype(ValueType::Expression), expression(std::move(value)) {}

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

public:
    enum class StatementType : int
    {
        If,
        For,
        Class,
        While,
        Switch,
        Foreach,
        Function,

        Assign,
        Incremental,

        Break,
        Return,
        Continue,

        CompondStatement,
    };

public:
    StatementType stype;

public:
    std::unique_ptr<AST::If> ifStatement;
    std::unique_ptr<AST::For> forStatement;
    std::unique_ptr<AST::Class> classStatement;
    std::unique_ptr<AST::While> whileStatement;
    std::unique_ptr<AST::Switch> switchStatement;
    std::unique_ptr<AST::Foreach> foreachStatement;
    std::unique_ptr<AST::Function> functionStatement;

public:
    std::unique_ptr<AST::Assign> assignStatement;
    std::unique_ptr<AST::Incremental> incrementalStatement;

public:
    std::unique_ptr<AST::Break> breakStatement;
    std::unique_ptr<AST::Return> returnStatement;
    std::unique_ptr<AST::Continue> continueStatement;

public:
    std::unique_ptr<AST::CompondStatement> compondStatement;

public:
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::If> &&value) : Node(Node::Type::Statement, token), stype(StatementType::If), ifStatement(std::move(value)) {}
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::For> &&value) : Node(Node::Type::Statement, token), stype(StatementType::For), forStatement(std::move(value)) {}
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::Class> &&value) : Node(Node::Type::Statement, token), stype(StatementType::Class), classStatement(std::move(value)) {}
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::While> &&value) : Node(Node::Type::Statement, token), stype(StatementType::While), whileStatement(std::move(value)) {}
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::Switch> &&value) : Node(Node::Type::Statement, token), stype(StatementType::Switch), switchStatement(std::move(value)) {}
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::Foreach> &&value) : Node(Node::Type::Statement, token), stype(StatementType::Foreach), foreachStatement(std::move(value)) {}
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::Function> &&value) : Node(Node::Type::Statement, token), stype(StatementType::Function), functionStatement(std::move(value)) {}

public:
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::Assign> &&value) : Node(Node::Type::Statement, token), stype(StatementType::Assign), assignStatement(std::move(value)) {}
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::Incremental> &&value) : Node(Node::Type::Statement, token), stype(StatementType::Incremental), incrementalStatement(std::move(value)) {}

public:
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::Break> &&value) : Node(Node::Type::Statement, token), stype(StatementType::Break), breakStatement(std::move(value)) {}
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::Return> &&value) : Node(Node::Type::Statement, token), stype(StatementType::Return), returnStatement(std::move(value)) {}
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::Continue> &&value) : Node(Node::Type::Statement, token), stype(StatementType::Continue), continueStatement(std::move(value)) {}

public:
    explicit Statement(const Token::Ptr &token, std::unique_ptr<AST::CompondStatement> &&value) : Node(Node::Type::Statement, token), stype(StatementType::CompondStatement), compondStatement(std::move(value)) {}

};

struct CompondStatement : public Node
{
    AST_NODE(CompondStatement)
    std::vector<std::unique_ptr<Statement>> statements;
};

/* end of #define AST_NODE(NodeName) ... */
#undef AST_NODE

/***** AST Visitor *****/

class Visitor
{

/*** Language Structures ***/

public:
    virtual void visitIf(const std::unique_ptr<If> &node);
    virtual void visitFor(const std::unique_ptr<For> &node);
    virtual void visitClass(const std::unique_ptr<Class> &node);
    virtual void visitWhile(const std::unique_ptr<While> &node);
    virtual void visitSwitch(const std::unique_ptr<Switch> &node);
    virtual void visitForeach(const std::unique_ptr<Foreach> &node);
    virtual void visitFunction(const std::unique_ptr<Function> &node);

public:
    virtual void visitAssign(const std::unique_ptr<Assign> &node);
    virtual void visitIncremental(const std::unique_ptr<Incremental> &node);

/*** Control Flows ***/

public:
    virtual void visitBreak(const std::unique_ptr<Break> &node) {}
    virtual void visitReturn(const std::unique_ptr<Return> &node);
    virtual void visitContinue(const std::unique_ptr<Continue> &node) {}

/*** Object Modifiers ***/

public:
    virtual void visitIndex(const std::unique_ptr<Index> &node);
    virtual void visitInvoke(const std::unique_ptr<Invoke> &node);
    virtual void visitAttribute(const std::unique_ptr<Attribute> &node);

/*** Composite Literals ***/

public:
    virtual void visitMap(const std::unique_ptr<Map> &node);
    virtual void visitArray(const std::unique_ptr<Array> &node);
    virtual void visitTuple(const std::unique_ptr<Tuple> &node);

/*** Expressions ***/

public:
    virtual void visitName(const std::unique_ptr<Name> &node) {}
    virtual void visitUnpack(const std::unique_ptr<Unpack> &node);
    virtual void visitLiteral(const std::unique_ptr<Literal> &node) {}
    virtual void visitComposite(const std::unique_ptr<Composite> &node);
    virtual void visitExpression(const std::unique_ptr<Expression> &node);

/*** Generic Statements ***/

public:
    virtual void visitStatement(const std::unique_ptr<Statement> &node);
    virtual void visitCompondStatement(const std::unique_ptr<CompondStatement> &node);

};
}

#endif /* REDSCRIPT_COMPILER_AST_H */
