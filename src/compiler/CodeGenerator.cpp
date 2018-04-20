#include <stdexcept>

#include "runtime/IntObject.h"
#include "runtime/CodeObject.h"
#include "runtime/StringObject.h"
#include "runtime/DecimalObject.h"

#include "runtime/InternalError.h"
#include "compiler/CodeGenerator.h"

namespace RedScript::Compiler
{
Runtime::ObjectRef CodeGenerator::build(void)
{
    CodeScope _(this, CodeType::FunctionCode);
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
            emitOperand(Engine::OpCode::STOR_LOCAL, addLocal(nameStr));
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
                emit(Engine::OpCode::SET_ITEM);
                break;
            }

            /* whatever(...) = value, impossible */
            case AST::Composite::ModType::Invoke:
                throw Runtime::InternalError("Impossible assignment to invocation");

            /* whatever.name = value */
            case AST::Composite::ModType::Attribute:
            {
                /* add the identifier into string table */
                auto name = node->mods.back().attribute->attr->name;
                uint32_t vid = addString(name);

                /* if this is inside a constructor and is the only modifier,
                 * and is setting attributes on `self`, it's defining an attribute */
                if (isInConstructor() && (node->mods.size() == 1) && (name == _firstArgName.top()))
                    emitOperand(Engine::OpCode::DEF_ATTR, vid);
                else
                    emitOperand(Engine::OpCode::SET_ATTR, vid);

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
    Visitor::visitIndex(node);
}

void CodeGenerator::visitInvoke(const std::unique_ptr<AST::Invoke> &node)
{
    Visitor::visitInvoke(node);
}

void CodeGenerator::visitAttribute(const std::unique_ptr<AST::Attribute> &node)
{
    Visitor::visitAttribute(node);
}

/*** Composite Literals ***/

void CodeGenerator::visitMap(const std::unique_ptr<AST::Map> &node)
{
    Visitor::visitMap(node);
}

void CodeGenerator::visitArray(const std::unique_ptr<AST::Array> &node)
{
    Visitor::visitArray(node);
}

void CodeGenerator::visitTuple(const std::unique_ptr<AST::Tuple> &node)
{
    Visitor::visitTuple(node);
}

/*** Expressions ***/

void CodeGenerator::visitName(const std::unique_ptr<AST::Name> &node)
{
    if (isLocal(node->name))
        emitOperand(Engine::OpCode::LOAD_LOCAL, addLocal(node->name));
    else
        emitOperand(Engine::OpCode::LOAD_GLOBAL, addString(node->name));
}

void CodeGenerator::visitUnpack(const std::unique_ptr<AST::Unpack> &node)
{
    /* check for item count, allows maximum 4G itms */
    if (node->items.size() > UINT32_MAX)
        throw Runtime::SyntaxError(node, "Too many items to unpack");

    /* expand the packed tuple into stack */
    emitOperand(
        Engine::OpCode::EXPAND_SEQ,
        static_cast<uint32_t>(node->items.size())
    );

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
    Runtime::ObjectRef val;

    /* construct corresponding type */
    switch (node->vtype)
    {
        case AST::Literal::Type::String  : val = Runtime::Object::newObject<Runtime::StringObject >(node->string ); break;
        case AST::Literal::Type::Decimal : val = Runtime::Object::newObject<Runtime::DecimalObject>(node->decimal); break;
        case AST::Literal::Type::Integer : val = Runtime::Object::newObject<Runtime::IntObject    >(node->integer); break;
    }

    /* emit opcode with operand */
    emitOperand(Engine::OpCode::LOAD_CONST, addConst(val));
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
        switch (item.op)
        {
            /* boolean or, with short-circuit evaluation */
            case Token::Operator::BoolOr:
            {
                patches.emplace_back(emitJump(Engine::OpCode::BRTRUE));
                break;
            }

            /* boolean and, with short-circuit evaluation */
            case Token::Operator::BoolAnd:
            {
                patches.emplace_back(emitJump(Engine::OpCode::BRFALSE));
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
        switch (item.op)
        {
            /* comparison operators */
            case Token::Operator::Less              : emit(Engine::OpCode::LE);          break;
            case Token::Operator::Greater           : emit(Engine::OpCode::GE);          break;
            case Token::Operator::Leq               : emit(Engine::OpCode::LEQ);         break;
            case Token::Operator::Geq               : emit(Engine::OpCode::LEQ);         break;
            case Token::Operator::Equ               : emit(Engine::OpCode::EQ);          break;
            case Token::Operator::Neq               : emit(Engine::OpCode::NEQ);         break;
            case Token::Operator::In                : emit(Engine::OpCode::IN);          break;

            /* boolean operators, with short-circuit evaluation */
            case Token::Operator::BoolOr            : emit(Engine::OpCode::BOOL_OR);     break;
            case Token::Operator::BoolAnd           : emit(Engine::OpCode::BOOL_AND);    break;

            /* basic arithmetic operators */
            case Token::Operator::Plus              : emit(Engine::OpCode::ADD);         break;
            case Token::Operator::Minus             : emit(Engine::OpCode::SUB);         break;
            case Token::Operator::Divide            : emit(Engine::OpCode::DIV);         break;
            case Token::Operator::Multiply          : emit(Engine::OpCode::MUL);         break;
            case Token::Operator::Module            : emit(Engine::OpCode::MOD);         break;
            case Token::Operator::Power             : emit(Engine::OpCode::POWER);       break;

            /* bit manipulation operators */
            case Token::Operator::BitAnd            : emit(Engine::OpCode::BIT_AND);     break;
            case Token::Operator::BitOr             : emit(Engine::OpCode::BIT_OR);      break;
            case Token::Operator::BitXor            : emit(Engine::OpCode::BIT_XOR);     break;

            /* bit shifting operators */
            case Token::Operator::ShiftLeft         : emit(Engine::OpCode::LSHIFT);      break;
            case Token::Operator::ShiftRight        : emit(Engine::OpCode::RSHIFT);      break;

            /* inplace basic arithmetic operators */
            case Token::Operator::InplaceAdd        : emit(Engine::OpCode::INP_ADD);     break;
            case Token::Operator::InplaceSub        : emit(Engine::OpCode::INP_SUB);     break;
            case Token::Operator::InplaceMul        : emit(Engine::OpCode::INP_MUL);     break;
            case Token::Operator::InplaceDiv        : emit(Engine::OpCode::INP_DIV);     break;
            case Token::Operator::InplaceMod        : emit(Engine::OpCode::INP_MOD);     break;
            case Token::Operator::InplacePower      : emit(Engine::OpCode::INP_POWER);   break;

            /* inplace bit manipulation operators */
            case Token::Operator::InplaceBitAnd     : emit(Engine::OpCode::INP_BIT_AND); break;
            case Token::Operator::InplaceBitOr      : emit(Engine::OpCode::INP_BIT_OR);  break;
            case Token::Operator::InplaceBitXor     : emit(Engine::OpCode::INP_BIT_XOR); break;

            /* inplace bit shifting operators */
            case Token::Operator::InplaceShiftLeft  : emit(Engine::OpCode::INP_LSHIFT);  break;
            case Token::Operator::InplaceShiftRight : emit(Engine::OpCode::INP_RSHIFT);  break;

            default:
                throw Runtime::InternalError(Utils::Strings::format("Impossible operator %s", Token::toString(item.op)));
        }
    }

    /* patch all jump instructions */
    for (const auto &p : patches)
        patchJump(p, buffer().size());
}
}
