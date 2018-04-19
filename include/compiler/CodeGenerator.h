#ifndef REDSCRIPT_COMPILER_CODEGENERATOR_H
#define REDSCRIPT_COMPILER_CODEGENERATOR_H

#include <memory>
#include <string>

#include "engine/Bytecode.h"
#include "compiler/AST.h"
#include "runtime/Object.h"

namespace RedScript::Compiler
{
class CodeGenerator : public AST::Visitor
{
    Runtime::ObjectRef _code;

private:
    std::vector<char> _buffer;
    std::vector<std::string> _strings;
    std::vector<Runtime::ObjectRef> _consts;
    std::unique_ptr<AST::CompoundStatement> _block;

public:
    virtual ~CodeGenerator() = default;
    explicit CodeGenerator(std::unique_ptr<AST::CompoundStatement> &&block) : _block(std::move(block)) {}

private:
    uint32_t emit(Engine::OpCode op);
    uint32_t addConst(Runtime::ObjectRef value);
    uint32_t addString(const std::string &value);

private:
    uint32_t emitJump(Engine::OpCode op);
    uint32_t emitOperand(Engine::OpCode op, int32_t operand);

public:
    Runtime::ObjectRef build(void);

    const std::vector<char> &buffer() const { return _buffer; }

protected:
    virtual void visitLiteral(const std::unique_ptr<AST::Literal> &node) override;
    virtual void visitExpression(const std::unique_ptr<AST::Expression> &node) override;

};
}

#endif /* REDSCRIPT_COMPILER_CODEGENERATOR_H */
