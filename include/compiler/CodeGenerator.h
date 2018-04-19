#ifndef REDSCRIPT_COMPILER_CODEGENERATOR_H
#define REDSCRIPT_COMPILER_CODEGENERATOR_H

#include <memory>
#include <string>

#include "compiler/AST.h"
#include "runtime/Object.h"

namespace RedScript::Compiler
{
class CodeGenerator : public AST::Visitor
{
    Runtime::ObjectRef _code;
    std::unique_ptr<AST::CompoundStatement> _block;

public:
    virtual ~CodeGenerator() = default;
    explicit CodeGenerator(std::unique_ptr<AST::CompoundStatement> &&block) : _block(std::move(block)) {}

public:
    Runtime::ObjectRef build(void);

protected:
    virtual void visitStatement(const std::unique_ptr<AST::Statement> &node) override;

};
}

#endif /* REDSCRIPT_COMPILER_CODEGENERATOR_H */
