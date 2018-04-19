#include "runtime/CodeObject.h"
#include "compiler/CodeGenerator.h"

namespace RedScript::Compiler
{
Runtime::ObjectRef CodeGenerator::build(void)
{
    /* generate code if not generated */
    if (_code.isNull())
    {
        _code = Runtime::Object::newObject<Runtime::CodeObject>();
        visitCompoundStatement(_block);
    }

    /* return the code reference */
    return _code;
}

void CodeGenerator::visitStatement(const std::unique_ptr<AST::Statement> &node)
{
    Visitor::visitStatement(node);
}
}
