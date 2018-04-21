#include <stdexcept>

#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/CodeObject.h"
#include "runtime/NullObject.h"
#include "runtime/StringObject.h"
#include "runtime/DecimalObject.h"

#include "runtime/InternalError.h"
#include "compiler/CodeGenerator.h"

namespace RedScript::Compiler
{
Runtime::ObjectRef CodeGenerator::build(void)
{
    /* enclosure the whole module into a pseudo-function */
    CodeScope cs(this, CodeType::FunctionCode);
    FunctionScope fs(this, nullptr, {});

    /* build the compound statement */
    visitCompoundStatement(_block);
    return std::move(_codeStack.back().second);
}

/*** Language Structures ***/

void CodeGenerator::visitIf(const std::unique_ptr<AST::If> &node)
{
    Visitor::visitIf(node);
}

void CodeGenerator::visitFor(const std::unique_ptr<AST::For> &node)
{
    Visitor::visitFor(node);
}

void CodeGenerator::visitTry(const std::unique_ptr<AST::Try> &node)
{
    Visitor::visitTry(node);
}

void CodeGenerator::visitClass(const std::unique_ptr<AST::Class> &node)
{
    CodeScope cls(this, CodeType::ClassCode);
    Visitor::visitClass(node);
}

void CodeGenerator::visitWhile(const std::unique_ptr<AST::While> &node)
{
    Visitor::visitWhile(node);
}

void CodeGenerator::visitNative(const std::unique_ptr<AST::Native> &node)
{
    Visitor::visitNative(node);
}

void CodeGenerator::visitSwitch(const std::unique_ptr<AST::Switch> &node)
{
    Visitor::visitSwitch(node);
}

void CodeGenerator::visitFunction(const std::unique_ptr<AST::Function> &node)
{
    /* create a new code frame and function scope */
    CodeScope func(this, CodeType::FunctionCode);
    FunctionScope _(this, node->name, node->args);

    Visitor::visitFunction(node);
}

bool CodeGenerator::isInConstructor(void)
{
    return (_codeStack.size() >= 2) &&                                  /* should have at least 2 code frames (class -> function) */
           ((_codeStack.end() - 2)->first == CodeType::ClassCode) &&    /* the 2nd to last frame must be a class */
           (_codeStack.back().first == CodeType::FunctionCode) &&       /* the last frame must be a function */
           (_currentFunctionName.top() == "__init__");                  /* which name must be "__init__" */
}

void CodeGenerator::buildCompositeTarget(const std::unique_ptr<AST::Composite> &node)
{
    /* should be mutable */
    if (!node->isSyntacticallyMutable())
        throw Runtime::InternalError("Immutable assign targets");

    /* a single name, set as local variable */
    if (node->mods.empty())
    {
        /* name node and string */
        auto &name = node->name;
        std::string nameStr = name->name;

        /* cannot re-assign `self` in constructors */
        if (!isInConstructor() || (nameStr != _firstArgName.top()))
            emitOperand(name, Engine::OpCode::STOR_LOCAL, addLocal(nameStr));
        else
            throw Runtime::SyntaxError(name, Utils::Strings::format("Cannot reassign \"%s\" in constructors", nameStr));
    }
    else
    {
        /* load the name into stack */
        visitName(node->name);

        /* generate all modifiers, except for the last one */
        for (size_t i = 0; i < node->mods.size() - 1; i++)
        {
            switch (node->mods[i].type)
            {
                case AST::Composite::ModType::Index: visitIndex(node->mods[i].index); break;
                case AST::Composite::ModType::Invoke: visitInvoke(node->mods[i].invoke); break;
                case AST::Composite::ModType::Attribute: visitAttribute(node->mods[i].attribute); break;
            }
        }

        /* and generate the last modifier */
        switch (node->mods.back().type)
        {
            /* whatever[expr] = value */
            case AST::Composite::ModType::Index:
            {
                visitExpression(node->mods.back().index->index);
                emit(node, Engine::OpCode::SET_ITEM);
                break;
            }

            /* whatever(...) = value, impossible */
            case AST::Composite::ModType::Invoke:
                throw Runtime::InternalError("Impossible assignment to invocation");

            /* whatever.name = value */
            case AST::Composite::ModType::Attribute:
            {
                /* add the identifier into string table */
                auto &attr = node->mods.back().attribute->attr;
                uint32_t vid = addString(attr->name);

                /* if this is inside a constructor and is the only modifier,
                 * and is setting attributes on `self`, it's defining an attribute */
                if (isInConstructor() && (node->mods.size() == 1) && (attr->name == _firstArgName.top()))
                    emitOperand(attr, Engine::OpCode::DEF_ATTR, vid);
                else
                    emitOperand(attr, Engine::OpCode::SET_ATTR, vid);

                break;
            }
        }
    }
}

void CodeGenerator::visitAssign(const std::unique_ptr<AST::Assign> &node)
{
    /* build the expression value */
    visitExpression(node->expression);

    /* check assigning target type */
    if (node->unpack)
        visitUnpack(node->unpack);
    else
        buildCompositeTarget(node->composite);
}

void CodeGenerator::visitIncremental(const std::unique_ptr<AST::Incremental> &node)
{
    Visitor::visitIncremental(node);
}

/*** Misc. Statements ***/

void CodeGenerator::visitRaise(const std::unique_ptr<AST::Raise> &node)
{
    Visitor::visitRaise(node);
}

void CodeGenerator::visitDelete(const std::unique_ptr<AST::Delete> &node)
{
    Visitor::visitDelete(node);
}

void CodeGenerator::visitImport(const std::unique_ptr<AST::Import> &node)
{
    Visitor::visitImport(node);
}

/*** Control Flows ***/

void CodeGenerator::visitBreak(const std::unique_ptr<AST::Break> &node)
{
    Visitor::visitBreak(node);
}

void CodeGenerator::visitReturn(const std::unique_ptr<AST::Return> &node)
{
    Visitor::visitReturn(node);
}

void CodeGenerator::visitContinue(const std::unique_ptr<AST::Continue> &node)
{
    Visitor::visitContinue(node);
}

/*** Object Modifiers ***/

void CodeGenerator::visitIndex(const std::unique_ptr<AST::Index> &node)
{
    visitExpression(node->index);
    emit(node, Engine::OpCode::GET_ITEM);
}

void CodeGenerator::visitInvoke(const std::unique_ptr<AST::Invoke> &node)
{
    /* function invocation flags */
    uint32_t flags = 0;

    /* normal arguments */
    if (!node->args.empty())
    {
        /* check item count */
        if (node->args.size() > UINT32_MAX)
            throw Runtime::SyntaxError(node, "Too many arguments");

        /* build as tuple */
        for (const auto &item : node->args)
            visitExpression(item);

        /* build a tuple around arguments */
        flags |= Engine::FI_ARGS;
        emitOperand(node, Engine::OpCode::MAKE_TUPLE, static_cast<uint32_t>(node->args.size()));
    }

    /* optional keyword arguments */
    if (!node->kwargs.empty())
    {
        /* check item count */
        if (node->kwargs.size() > UINT32_MAX)
            throw Runtime::SyntaxError(node, "Too many keyword arguments");

        /* build as key value pair map */
        for (const auto &item : node->kwargs)
        {
            /* wrap key as string object */
            auto name = item.first->name;
            auto string = Runtime::Reference<Runtime::StringObject>::newObject(name);

            /* load key and value into stack */
            emitOperand(node, Engine::OpCode::LOAD_CONST, addConst(string));
            visitExpression(item.second);
        }

        /* build a map around keyword arguments */
        flags |= Engine::FI_NAMED;
        emitOperand(node, Engine::OpCode::MAKE_MAP, static_cast<uint32_t>(node->kwargs.size()));
    }

    /* optional variable arguments */
    if (node->varg)
    {
        flags |= Engine::FI_VARGS;
        visitExpression(node->varg);
    }

    /* optional variable arguments */
    if (node->kwarg)
    {
        flags |= Engine::FI_KWARGS;
        visitExpression(node->kwarg);
    }

    /* emit the function invocation instruction */
    emitOperand(node, Engine::OpCode::CALL_FUNCTION, flags);
}

void CodeGenerator::visitAttribute(const std::unique_ptr<AST::Attribute> &node)
{
    /* stack_top = stack_top.attr */
    emitOperand(node, Engine::OpCode::GET_ATTR, addString(node->attr->name));
}

/*** Composite Literals ***/

void CodeGenerator::visitMap(const std::unique_ptr<AST::Map> &node)
{
    /* check item count */
    if (node->items.size() > UINT32_MAX)
        throw Runtime::SyntaxError(node, "Too many map items");

    /* build each item expression */
    for (const auto &item : node->items)
    {
        visitExpression(item.first);
        visitExpression(item.second);
    }

    /* pack into a map */
    emitOperand(node, Engine::OpCode::MAKE_MAP, static_cast<uint32_t>(node->items.size()));
}

void CodeGenerator::visitArray(const std::unique_ptr<AST::Array> &node)
{
    /* check item count */
    if (node->items.size() > UINT32_MAX)
        throw Runtime::SyntaxError(node, "Too many array items");

    /* build each item expression, in reverse order */
    for (const auto &item : node->items)
        visitExpression(item);

    /* pack into an array */
    emitOperand(node, Engine::OpCode::MAKE_ARRAY, static_cast<uint32_t>(node->items.size()));
}

void CodeGenerator::visitTuple(const std::unique_ptr<AST::Tuple> &node)
{
    /* check item count */
    if (node->items.size() > UINT32_MAX)
        throw Runtime::SyntaxError(node, "Too many tuple items");

    /* build each item expression, in reverse order */
    for (const auto &item : node->items)
        visitExpression(item);

    /* pack into a tuple */
    emitOperand(node, Engine::OpCode::MAKE_TUPLE, static_cast<uint32_t>(node->items.size()));
}

/*** Expressions ***/

void CodeGenerator::visitName(const std::unique_ptr<AST::Name> &node)
{
    if (isLocal(node->name))
        emitOperand(node, Engine::OpCode::LOAD_LOCAL, addLocal(node->name));
    else
        emitOperand(node, Engine::OpCode::LOAD_GLOBAL, addString(node->name));
}

void CodeGenerator::visitUnpack(const std::unique_ptr<AST::Unpack> &node)
{
    /* check for item count, allows maximum 4G items */
    if (node->items.size() > UINT32_MAX)
        throw Runtime::SyntaxError(node, "Too many items to unpack");

    /* expand the packed tuple into stack */
    emitOperand(node, Engine::OpCode::EXPAND_SEQ, static_cast<uint32_t>(node->items.size()));

    /* build each item */
    for (const auto &item : node->items)
    {
        switch (item.type)
        {
            case AST::Unpack::Target::Type::Subset    : visitUnpack(item.subset);             break;
            case AST::Unpack::Target::Type::Composite : buildCompositeTarget(item.composite); break;
        }
    }
}

void CodeGenerator::visitLiteral(const std::unique_ptr<AST::Literal> &node)
{
    /* constant value reference */
    Runtime::ObjectRef val;

    /* construct corresponding type */
    switch (node->vtype)
    {
        case AST::Literal::Type::Integer : val = Runtime::IntObject::fromInt(node->integer); break;
        case AST::Literal::Type::String  : val = Runtime::StringObject::fromString(node->string); break;
        case AST::Literal::Type::Decimal : val = Runtime::DecimalObject::fromDouble(node->decimal); break;
    }

    /* emit opcode with operand */
    emitOperand(node, Engine::OpCode::LOAD_CONST, addConst(val));
}

void CodeGenerator::visitDecorator(const std::unique_ptr<AST::Decorator> &node)
{
    Visitor::visitDecorator(node);
}

void CodeGenerator::visitExpression(const std::unique_ptr<AST::Expression> &node)
{
    std::vector<size_t> patches;

    /* first expression */
    switch (node->first.type)
    {
        case AST::Expression::Operand::Type::Composite  : visitComposite(node->first.composite); break;
        case AST::Expression::Operand::Type::Expression : visitExpression(node->first.expression); break;
    }

    /* remaining expression parts */
    for (const auto &item : node->follows)
    {
        /* short-circuited boolean operation */
        switch (item.op->asOperator())
        {
            /* boolean or, with short-circuit evaluation */
            case Token::Operator::BoolOr:
            {
                patches.emplace_back(emitJump(item.op, Engine::OpCode::BRTRUE));
                break;
            }

            /* boolean and, with short-circuit evaluation */
            case Token::Operator::BoolAnd:
            {
                patches.emplace_back(emitJump(item.op, Engine::OpCode::BRFALSE));
                break;
            }

            /* other operators, don't care */
            default:
                break;
        }

        /* next term */
        switch (item.type)
        {
            case AST::Expression::Operand::Type::Composite  : visitComposite(item.composite); break;
            case AST::Expression::Operand::Type::Expression : visitExpression(item.expression); break;
        }

        /* emit operators */
        switch (item.op->asOperator())
        {
            /* comparison operators */
            case Token::Operator::Less              : emit(item.op, Engine::OpCode::LE);          break;
            case Token::Operator::Greater           : emit(item.op, Engine::OpCode::GE);          break;
            case Token::Operator::Leq               : emit(item.op, Engine::OpCode::LEQ);         break;
            case Token::Operator::Geq               : emit(item.op, Engine::OpCode::LEQ);         break;
            case Token::Operator::Equ               : emit(item.op, Engine::OpCode::EQ);          break;
            case Token::Operator::Neq               : emit(item.op, Engine::OpCode::NEQ);         break;
            case Token::Operator::In                : emit(item.op, Engine::OpCode::IN);          break;

            /* boolean operators, with short-circuit evaluation */
            case Token::Operator::BoolOr            : emit(item.op, Engine::OpCode::BOOL_OR);     break;
            case Token::Operator::BoolAnd           : emit(item.op, Engine::OpCode::BOOL_AND);    break;

            /* basic arithmetic operators */
            case Token::Operator::Plus              : emit(item.op, Engine::OpCode::ADD);         break;
            case Token::Operator::Minus             : emit(item.op, Engine::OpCode::SUB);         break;
            case Token::Operator::Divide            : emit(item.op, Engine::OpCode::DIV);         break;
            case Token::Operator::Multiply          : emit(item.op, Engine::OpCode::MUL);         break;
            case Token::Operator::Module            : emit(item.op, Engine::OpCode::MOD);         break;
            case Token::Operator::Power             : emit(item.op, Engine::OpCode::POWER);       break;

            /* bit manipulation operators */
            case Token::Operator::BitAnd            : emit(item.op, Engine::OpCode::BIT_AND);     break;
            case Token::Operator::BitOr             : emit(item.op, Engine::OpCode::BIT_OR);      break;
            case Token::Operator::BitXor            : emit(item.op, Engine::OpCode::BIT_XOR);     break;

            /* bit shifting operators */
            case Token::Operator::ShiftLeft         : emit(item.op, Engine::OpCode::LSHIFT);      break;
            case Token::Operator::ShiftRight        : emit(item.op, Engine::OpCode::RSHIFT);      break;

            /* inplace basic arithmetic operators */
            case Token::Operator::InplaceAdd        : emit(item.op, Engine::OpCode::INP_ADD);     break;
            case Token::Operator::InplaceSub        : emit(item.op, Engine::OpCode::INP_SUB);     break;
            case Token::Operator::InplaceMul        : emit(item.op, Engine::OpCode::INP_MUL);     break;
            case Token::Operator::InplaceDiv        : emit(item.op, Engine::OpCode::INP_DIV);     break;
            case Token::Operator::InplaceMod        : emit(item.op, Engine::OpCode::INP_MOD);     break;
            case Token::Operator::InplacePower      : emit(item.op, Engine::OpCode::INP_POWER);   break;

            /* inplace bit manipulation operators */
            case Token::Operator::InplaceBitAnd     : emit(item.op, Engine::OpCode::INP_BIT_AND); break;
            case Token::Operator::InplaceBitOr      : emit(item.op, Engine::OpCode::INP_BIT_OR);  break;
            case Token::Operator::InplaceBitXor     : emit(item.op, Engine::OpCode::INP_BIT_XOR); break;

            /* inplace bit shifting operators */
            case Token::Operator::InplaceShiftLeft  : emit(item.op, Engine::OpCode::INP_LSHIFT);  break;
            case Token::Operator::InplaceShiftRight : emit(item.op, Engine::OpCode::INP_RSHIFT);  break;

            /* other operators, should not happen */
            default:
                throw Runtime::InternalError(Utils::Strings::format("Impossible operator %s", item.op->toString()));
        }
    }

    /* patch all jump instructions */
    for (const auto &p : patches)
        patchJump(p, buffer().size());
}
}
