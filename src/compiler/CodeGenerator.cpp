#include <stdexcept>

#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/CodeObject.h"
#include "runtime/NullObject.h"
#include "runtime/StringObject.h"
#include "runtime/DecimalObject.h"

#include "compiler/CodeGenerator.h"

namespace RedScript::Compiler
{
static inline Engine::OpCode incOpCode(Token::Operator op)
{
    switch (op)
    {
        case Token::Operator::InplaceAdd        : return Engine::OpCode::INP_ADD;
        case Token::Operator::InplaceSub        : return Engine::OpCode::INP_SUB;
        case Token::Operator::InplaceMul        : return Engine::OpCode::INP_MUL;
        case Token::Operator::InplaceDiv        : return Engine::OpCode::INP_DIV;
        case Token::Operator::InplaceMod        : return Engine::OpCode::INP_MOD;
        case Token::Operator::InplacePower      : return Engine::OpCode::INP_POWER;
        case Token::Operator::InplaceBitOr      : return Engine::OpCode::INP_BIT_OR;
        case Token::Operator::InplaceBitAnd     : return Engine::OpCode::INP_BIT_AND;
        case Token::Operator::InplaceBitXor     : return Engine::OpCode::INP_BIT_XOR;
        case Token::Operator::InplaceShiftLeft  : return Engine::OpCode::INP_LSHIFT;
        case Token::Operator::InplaceShiftRight : return Engine::OpCode::INP_RSHIFT;

        default:
            throw Runtime::InternalError("Impossible incremental operator");
    }
}

Runtime::ObjectRef CodeGenerator::build(void)
{
    /* enclosure the whole module into a pseudo-function */
    CodeScope cs(this, CodeType::FunctionCode);
    FunctionScope fs(this, nullptr, {});

    /* build the compound statement */
    visitCompoundStatement(_block);
    return std::move(_codeStack.back().code);
}

/*** Language Structures ***/

void CodeGenerator::buildClassObject(const std::unique_ptr<AST::Class> &node)
{
    // TODO: generate class
    CodeScope cls(this, CodeType::ClassCode);
    Visitor::visitClass(node);
}

void CodeGenerator::buildFunctionObject(const std::unique_ptr<AST::Function> &node)
{
    /* check for default value count */
    if (node->defaults.size() > UINT32_MAX)
        throw Runtime::RuntimeError("Too many default values");

    /* evaluate all default values when build functions */
    for (const auto &expr : node->defaults)
        visitExpression(expr);

    /* build as tuple */
    emitOperand(
        node,
        Engine::OpCode::MAKE_TUPLE,
        static_cast<uint32_t>(node->defaults.size())
    );

    /* create a new code frame and function scope */
    CodeScope func(this, CodeType::FunctionCode);
    FunctionScope _(this, node->name, node->args);

    /* add all names into local variable table */
    for (const auto &name : node->args)
        addLocal(name->name);

    /* also add vargs if any */
    if (node->vargs)
        addLocal(node->vargs->name);

    /* also add kwargs if any */
    if (node->kwargs)
        addLocal(node->kwargs->name);

    /* build function body, with a default return statement */
    visitStatement(node->body);
    emitOperand(node->body, Engine::OpCode::LOAD_CONST, addConst(Runtime::NullObject));
    emit(node->body, Engine::OpCode::POP_RETURN);
    emitOperand(node, Engine::OpCode::MAKE_FUNCTION, addConst(func.leave()));
}

void CodeGenerator::visitIf(const std::unique_ptr<AST::If> &node)
{
    /* if (<expr>) <stmt> */
    visitExpression(node->expr);
    uint32_t then = emitJump(node, Engine::OpCode::BRFALSE);
    visitStatement(node->positive);

    /* no else clause */
    if (!(node->negative))
    {
        patchBranch(then, pc());
        return;
    }

    /* else <stmt> */
    uint32_t exit = emitJump(node, Engine::OpCode::BR);
    patchBranch(then, pc());
    visitStatement(node->negative);
    patchBranch(exit, pc());
}

void CodeGenerator::visitFor(const std::unique_ptr<AST::For> &node)
{
    /* build the expression and make an iterator out of it */
    visitExpression(node->expr);
    emit(node->expr, Engine::OpCode::MAKE_ITER);

    /* advance the iterator */
    uint32_t entry = pc();
    emit(node->expr, Engine::OpCode::DUP);
    uint32_t exit = emitJump(node->expr, Engine::OpCode::ITER_NEXT);

    /* save to loop variables */
    if (node->pack)
        visitUnpack(node->pack);
    else
        buildCompositeTarget(node->comp);

    /* enter loop scope */
    BreakableScope bs(this);
    ContinuableScope cs(this);

    /* loop body */
    visitStatement(node->body);
    emitOperand(node, Engine::OpCode::BR, entry);

    /* leave the two scopes */
    auto breakBranches = bs.leave();
    auto continueBranches = cs.leave();

    /* patch all "continue" branches */
    for (uint32_t offset : continueBranches)
        patchBranch(offset, entry);

    /* control reaches here if iterator drained */
    patchBranch(exit, pc());

    /* optional "else" clause */
    if (node->branch)
        visitStatement(node->branch);

    /* patch all "break" branches, after "else" clause */
    for (uint32_t offset : breakBranches)
        patchBranch(offset, pc());
}

void CodeGenerator::visitTry(const std::unique_ptr<AST::Try> &node)
{
    // TODO: generate try
    Visitor::visitTry(node);
}

void CodeGenerator::visitClass(const std::unique_ptr<AST::Class> &node)
{
    buildClassObject(node);
    emitOperand(node->name, Engine::OpCode::STOR_LOCAL, addLocal(node->name->name));
}

void CodeGenerator::visitWhile(const std::unique_ptr<AST::While> &node)
{
    /* while condition expression */
    uint32_t entry = pc();
    visitExpression(node->expr);
    uint32_t exit = emitJump(node->expr, Engine::OpCode::BRFALSE);

    /* enter loop scope */
    BreakableScope bs(this);
    ContinuableScope cs(this);

    /* loop body */
    visitStatement(node->body);
    emitOperand(node, Engine::OpCode::BR, entry);

    /* leave the two scopes */
    auto breakBranches = bs.leave();
    auto continueBranches = cs.leave();

    /* patch all "continue" branches */
    for (uint32_t offset : continueBranches)
        patchBranch(offset, entry);

    /* control reaches here if iterator drained */
    patchBranch(exit, pc());

    /* optional "else" clause */
    if (node->branch)
        visitStatement(node->branch);

    /* patch all "break" branches, after "else" clause */
    for (uint32_t offset : breakBranches)
        patchBranch(offset, pc());
}

void CodeGenerator::visitNative(const std::unique_ptr<AST::Native> &node)
{
    // TODO: generate native
    Visitor::visitNative(node);
}

void CodeGenerator::visitSwitch(const std::unique_ptr<AST::Switch> &node)
{
    // TODO: generate switch
    Visitor::visitSwitch(node);
}

void CodeGenerator::visitFunction(const std::unique_ptr<AST::Function> &node)
{
    buildFunctionObject(node);
    emitOperand(node->name, Engine::OpCode::STOR_LOCAL, addLocal(node->name->name));
}

bool CodeGenerator::isInConstructor(void)
{
    return (_codeStack.size() >= 2) &&                                  /* should have at least 2 code frames (class -> function) */
           ((_codeStack.end() - 2)->type == CodeType::ClassCode) &&     /* the 2nd to last frame must be a class */
           (_codeStack.back().type == CodeType::FunctionCode) &&        /* the last frame must be a function */
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
    /* must be mutable */
    if (!(node->dest->isSyntacticallyMutable()))
        throw Runtime::InternalError("Immutable assigning target");

    /* generate incremental expression */
    auto visitIncrementalExpr = [&]
    {
        visitExpression(node->expr);
        emit(node->op, incOpCode(node->op->asOperator()));
    };

    /* composite base term */
    switch (node->dest->vtype)
    {
        case AST::Composite::ValueType::Map        : visitMap(node->dest->map); break;
        case AST::Composite::ValueType::Name       : visitName(node->dest->name); break;
        case AST::Composite::ValueType::Array      : visitArray(node->dest->array); break;
        case AST::Composite::ValueType::Tuple      : visitTuple(node->dest->tuple); break;
        case AST::Composite::ValueType::Literal    : visitLiteral(node->dest->literal); break;
        case AST::Composite::ValueType::Function   : visitFunction(node->dest->function); break;
        case AST::Composite::ValueType::Expression : visitExpression(node->dest->expression); break;
    }

    /* no modifiers, it's a pure name */
    if (node->dest->mods.empty())
    {
        /* add target attribute to string list */
        auto &name = node->dest->name;
        uint32_t vid = addString(name->name);

        /* in this case, must be local name */
        if (!isLocal(name->name))
            throw Runtime::SyntaxError(node, Utils::Strings::format("Local name \"%s\" referenced before assignment", name->name));

        /* generate name incremental assignment */
        visitIncrementalExpr();
        emitOperand(node->dest->name, Engine::OpCode::STOR_LOCAL, vid);
    }
    else
    {
        /* last modifier */
        AST::Composite::Modifier &mod = node->dest->mods.back();

        /* composite modifiers, except the last one */
        for (size_t i = 0; i < node->dest->mods.size() - 1; i++)
        {
            switch (node->dest->mods[i].type)
            {
                case AST::Composite::ModType::Index     : visitIndex(node->dest->mods[i].index); break;
                case AST::Composite::ModType::Invoke    : visitInvoke(node->dest->mods[i].invoke); break;
                case AST::Composite::ModType::Attribute : visitAttribute(node->dest->mods[i].attribute); break;
            }
        }

        /* incremental assigment needs special treatment here */
        switch (mod.type)
        {
            /* a[x] += ... */
            case AST::Composite::ModType::Index:
            {
                visitExpression(mod.index->index);
                emit(mod.index->index, Engine::OpCode::DUP2);
                emit(mod.index->index, Engine::OpCode::GET_ITEM);
                visitIncrementalExpr();
                emit(mod.index->index, Engine::OpCode::SET_ITEM);
                break;
            }

            /* a(...) = ..., impossible */
            case AST::Composite::ModType::Invoke:
                throw Runtime::InternalError("Immutable assigning target");

            /* a.b = ... */
            case AST::Composite::ModType::Attribute:
            {
                /* add target attribute to string list */
                auto &attr = mod.attribute->attr;
                uint32_t vid = addString(attr->name);

                /* generate attribute incremental assignment */
                emit(mod.attribute->attr, Engine::OpCode::DUP);
                emitOperand(mod.attribute->attr, Engine::OpCode::GET_ATTR, vid);
                visitIncrementalExpr();
                emitOperand(mod.attribute->attr, Engine::OpCode::SET_ATTR, vid);
                break;
            }
        }
    }
}

/*** Misc. Statements ***/

void CodeGenerator::visitRaise(const std::unique_ptr<AST::Raise> &node)
{
    visitExpression(node->expr);
    emit(node, Engine::OpCode::RAISE);
}

void CodeGenerator::visitDelete(const std::unique_ptr<AST::Delete> &node)
{
    // TODO: generate delete
    Visitor::visitDelete(node);
}

void CodeGenerator::visitImport(const std::unique_ptr<AST::Import> &node)
{
    // TODO: generate import
    Visitor::visitImport(node);
}

/*** Control Flows ***/

void CodeGenerator::visitBreak(const std::unique_ptr<AST::Break> &node)
{
    if (breakStack().empty())
        throw Runtime::InternalError("Not breakable here");
    else
        breakStack().top().emplace_back(emitJump(node, Engine::OpCode::BR));
}

void CodeGenerator::visitReturn(const std::unique_ptr<AST::Return> &node)
{
    visitExpression(node->value);
    emit(node, Engine::OpCode::POP_RETURN);
}

void CodeGenerator::visitContinue(const std::unique_ptr<AST::Continue> &node)
{
    if (continueStack().empty())
        throw Runtime::InternalError("Not continuable here");
    else
        continueStack().top().emplace_back(emitJump(node, Engine::OpCode::BR));
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
    /* decorative expression */
    AST::Name *name;
    visitExpression(node->expression);

    /* value to be decorated */
    switch (node->decoration)
    {
        case AST::Decorator::Decoration::Class:
        {
            name = node->klass->name.get();
            buildClassObject(node->klass);
            break;
        }

        case AST::Decorator::Decoration::Function:
        {
            name = node->function->name.get();
            buildFunctionObject(node->function);
            break;
        }
    }

    /* invoke decorator */
    emitOperand(node, Engine::OpCode::CALL_FUNCTION, Engine::FI_DECORATOR);
    emitOperand(name, Engine::OpCode::STOR_LOCAL, addLocal(name->name));
}

void CodeGenerator::visitExpression(const std::unique_ptr<AST::Expression> &node)
{
    /* short circuit patches */
    std::vector<uint32_t> patches;

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
    for (const auto &offset : patches)
        patchBranch(offset, pc());
}
}
