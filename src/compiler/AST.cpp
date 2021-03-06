#include "compiler/AST.h"

namespace RedScript::Compiler::AST
{
/*** Language Structures ***/

void Visitor::visitIf(const std::unique_ptr<If> &node)
{
    visitExpression(node->expr);
    visitStatement(node->positive);

    if (node->negative)
        visitStatement(node->negative);
}

void Visitor::visitFor(const std::unique_ptr<For> &node)
{
    if (node->pack)
        visitUnpack(node->pack);
    else
        visitComposite(node->comp);

    visitExpression(node->expr);
    visitStatement(node->body);

    if (node->branch)
        visitStatement(node->branch);
}

void Visitor::visitTry(const std::unique_ptr<Try> &node)
{
    visitStatement(node->body);

    for (const auto &except : node->excepts)
    {
        visitExpression(except.exception);

        if (except.alias)
            visitName(except.alias);

        visitStatement(except.handler);
    }

    if (node->finally)
        visitStatement(node->finally);
}

void Visitor::visitClass(const std::unique_ptr<Class> &node)
{
    visitName(node->name);

    if (node->super)
        visitExpression(node->super);

    visitStatement(node->body);
}

void Visitor::visitWhile(const std::unique_ptr<While> &node)
{
    visitExpression(node->expr);
    visitStatement(node->body);

    if (node->branch)
        visitStatement(node->branch);
}

void Visitor::visitNative(const std::unique_ptr<Native> &node)
{
    visitName(node->name);

    for (const auto &pair : node->opts)
    {
        visitName(pair.name);
        visitExpression(pair.value);
    }
}

void Visitor::visitSwitch(const std::unique_ptr<Switch> &node)
{
    visitExpression(node->expr);

    for (const auto &item : node->cases)
    {
        visitExpression(item.value);
        visitStatement(item.body);
    }

    if (node->def)
        visitStatement(node->def);
}

void Visitor::visitFunction(const std::unique_ptr<Function> &node)
{
    if (node->name)
        visitName(node->name);

    if (node->vargs)
        visitName(node->vargs);

    if (node->kwargs)
        visitName(node->kwargs);

    for (const auto &item : node->args)
        visitName(item);

    visitStatement(node->body);
}

void Visitor::visitAssign(const std::unique_ptr<Assign> &node)
{
    for (const auto &target : node->targets)
    {
        if (target.unpack)
            visitUnpack(target.unpack);
        else
            visitComposite(target.composite);
    }

    visitExpression(node->expression);
}

void Visitor::visitIncremental(const std::unique_ptr<Incremental> &node)
{
    visitComposite(node->dest);
    visitExpression(node->expr);
}

/*** Misc. Statements ***/

void Visitor::visitRaise(const std::unique_ptr<Raise> &node)
{
    visitExpression(node->expr);
}

void Visitor::visitDelete(const std::unique_ptr<Delete> &node)
{
    visitComposite(node->comp);
}

void Visitor::visitImport(const std::unique_ptr<Import> &node)
{
    if (node->alias)
        visitName(node->alias);

    for (const auto &name : node->names)
        visitName(name);
}

/*** Control Flows ***/

void Visitor::visitReturn(const std::unique_ptr<Return> &node)
{
    visitExpression(node->value);
}

/*** Object Modifiers ***/

void Visitor::visitIndex(const std::unique_ptr<Index> &node)
{
    visitExpression(node->index);
}

void Visitor::visitSlice(const std::unique_ptr<Slice> &node)
{
    if (node->begin)
        visitExpression(node->begin);

    if (node->end)
        visitExpression(node->end);

    if (node->step)
        visitExpression(node->step);
}

void Visitor::visitInvoke(const std::unique_ptr<Invoke> &node)
{
    for (const auto &arg : node->args)
        visitExpression(arg);

    for (const auto &kwarg : node->kwargs)
    {
        visitName(kwarg.first);
        visitExpression(kwarg.second);
    }

    if (node->varg)
        visitExpression(node->varg);

    if (node->kwarg)
        visitExpression(node->kwarg);
}

void Visitor::visitAttribute(const std::unique_ptr<Attribute> &node)
{
    visitName(node->attr);
}

/*** Composite Literals ***/

void Visitor::visitMap(const std::unique_ptr<Map> &node)
{
    for (const auto &item : node->items)
    {
        visitExpression(item.first);
        visitExpression(item.second);
    }
}

void Visitor::visitArray(const std::unique_ptr<Array> &node)
{
    for (const auto &item : node->items)
        visitExpression(item);
}

void Visitor::visitTuple(const std::unique_ptr<Tuple> &node)
{
    for (const auto &item : node->items)
        visitExpression(item);
}

/*** Expressions ***/

void Visitor::visitUnpack(const std::unique_ptr<Unpack> &node)
{
    for (const auto &item : node->items)
    {
        switch (item.type)
        {
            case Unpack::Target::Type::Subset    : visitUnpack(item.subset); break;
            case Unpack::Target::Type::Composite : visitComposite(item.composite); break;
        }
    }
}

void Visitor::visitComposite(const std::unique_ptr<Composite> &node)
{
    switch (node->vtype)
    {
        case Composite::ValueType::Map        : visitMap(node->map); break;
        case Composite::ValueType::Name       : visitName(node->name); break;
        case Composite::ValueType::Array      : visitArray(node->array); break;
        case Composite::ValueType::Tuple      : visitTuple(node->tuple); break;
        case Composite::ValueType::Literal    : visitLiteral(node->literal); break;
        case Composite::ValueType::Function   : visitFunction(node->function); break;
        case Composite::ValueType::Expression : visitExpression(node->expression); break;
    }

    for (const auto &mod : node->mods)
    {
        switch (mod.type)
        {
            case Composite::ModType::Index     : visitIndex(mod.index); break;
            case Composite::ModType::Slice     : visitSlice(mod.slice); break;
            case Composite::ModType::Invoke    : visitInvoke(mod.invoke); break;
            case Composite::ModType::Attribute : visitAttribute(mod.attribute); break;
        }
    }
}

void Visitor::visitDecorator(const std::unique_ptr<Decorator> &node)
{
    switch (node->decoration)
    {
        case Decorator::Decoration::Class    : visitClass(node->klass); break;
        case Decorator::Decoration::Function : visitFunction(node->function); break;
    }

    visitExpression(node->expression);
}

void Visitor::visitExpression(const std::unique_ptr<Expression> &node)
{
    switch (node->first.type)
    {
        case Expression::Operand::Type::Composite  : visitComposite(node->first.composite); break;
        case Expression::Operand::Type::Expression : visitExpression(node->first.expression); break;
    }

    for (const auto &item : node->follows)
    {
        switch (item.type)
        {
            case Expression::Operand::Type::Composite  : visitComposite(item.composite); break;
            case Expression::Operand::Type::Expression : visitExpression(item.expression); break;
        }
    }
}

/*** Generic Statements ***/

void Visitor::visitStatement(const std::unique_ptr<Statement> &node)
{
    switch (node->stype)
    {
        case Statement::StatementType::If                : visitIf(node->ifStatement); break;
        case Statement::StatementType::For               : visitFor(node->forStatement); break;
        case Statement::StatementType::Try               : visitTry(node->tryStatement); break;
        case Statement::StatementType::Class             : visitClass(node->classStatement); break;
        case Statement::StatementType::While             : visitWhile(node->whileStatement); break;
        case Statement::StatementType::Native            : visitNative(node->nativeStatement); break;
        case Statement::StatementType::Switch            : visitSwitch(node->switchStatement); break;
        case Statement::StatementType::Function          : visitFunction(node->functionStatement); break;

        case Statement::StatementType::Assign            : visitAssign(node->assignStatement); break;
        case Statement::StatementType::Incremental       : visitIncremental(node->incrementalStatement); break;

        case Statement::StatementType::Raise             : visitRaise(node->raiseStatement); break;
        case Statement::StatementType::Delete            : visitDelete(node->deleteStatement); break;
        case Statement::StatementType::Import            : visitImport(node->importStatement); break;

        case Statement::StatementType::Break             : visitBreak(node->breakStatement); break;
        case Statement::StatementType::Return            : visitReturn(node->returnStatement); break;
        case Statement::StatementType::Continue          : visitContinue(node->continueStatement); break;

        case Statement::StatementType::Decorator         : visitDecorator(node->decorator); break;
        case Statement::StatementType::Expression        : visitExpression(node->expression); break;
        case Statement::StatementType::CompoundStatement : visitCompoundStatement(node->compoundStatement); break;
    }
}

void Visitor::visitCompoundStatement(const std::unique_ptr<CompoundStatement> &node)
{
    for (const auto &stmt : node->statements)
        visitStatement(stmt);
}
}
