#ifndef REDSCRIPT_COMPILER_CODEGENERATOR_H
#define REDSCRIPT_COMPILER_CODEGENERATOR_H

#include <stack>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "engine/Bytecode.h"
#include "compiler/AST.h"
#include "runtime/Object.h"
#include "runtime/CodeObject.h"

namespace RedScript::Compiler
{
class CodeGenerator : public AST::Visitor
{
    enum class CodeType : int
    {
        ClassCode,
        FunctionCode,
    };

private:
    std::stack<std::string> _firstArgName;
    std::stack<std::string> _currentFunctionName;

private:
    std::unique_ptr<AST::CompoundStatement> _block;
    std::vector<std::pair<CodeType, Runtime::ObjectRef>> _codeStack;

public:
    virtual ~CodeGenerator() = default;
    explicit CodeGenerator(std::unique_ptr<AST::CompoundStatement> &&block) : _block(std::move(block)) {}

protected:
    typedef Runtime::Reference<Runtime::CodeObject> CodeRef;
    inline CodeRef code(void) const { return _codeStack.back().second.as<Runtime::CodeObject>(); }

protected:
    inline const std::vector<char> &buffer(void) const { return code()->buffer(); }
    inline const std::vector<std::string> &names(void) const { return code()->names(); }
    inline const std::vector<Runtime::ObjectRef> &consts(void) const { return code()->consts(); }

protected:
    inline uint32_t emit(Engine::OpCode op) { return code()->emit(op); }
    inline uint32_t addConst(Runtime::ObjectRef value) { return code()->addConst(value); }
    inline uint32_t addLocal(const std::string &value) { return code()->addLocal(value); }
    inline uint32_t addString(const std::string &value) { return code()->addString(value); }

protected:
    inline uint32_t emitJump(Engine::OpCode op) { return code()->emitJump(op); }
    inline uint32_t emitOperand(Engine::OpCode op, int32_t operand) { return code()->emitOperand(op, operand); }

protected:
    inline bool isLocal(const std::string &value) const { return code()->isLocal(value); }
    inline void patchJump(size_t offset, size_t address) { code()->patchJump(offset, address); }

protected:
    class CodeScope final
    {
        bool _left;
        CodeGenerator *_self;

    public:
       ~CodeScope() { if (!_left) _self->_codeStack.pop_back(); }
        CodeScope(CodeGenerator *self, CodeType type) : _left(false), _self(self)
        {
            /* create a new code object on the stack top */
            self->_codeStack.emplace_back({type, Runtime::Object::newObject<Runtime::CodeObject>()});
        }

    public:
        Runtime::ObjectRef leave(void)
        {
            /* already left */
            if (_left)
                return nullptr;

            /* get the most recent code object */
            auto &code = _self->_codeStack;
            Runtime::ObjectRef ref = std::move(code.back().second);

            /* pop from code stack */
            _left = true;
            code.pop_back();
            return std::move(ref);
        }
    };

protected:
    class FunctionScope final
    {
        std::stack<std::string> &_args;
        std::stack<std::string> &_funcs;

    public:
        ~FunctionScope()
        {
            _args.pop();
            _funcs.pop();
        }

    public:
        FunctionScope(
            CodeGenerator *self,
            std::unique_ptr<AST::Name> &name,
            std::vector<std::unique_ptr<AST::Name>> &args
        ) : _args(self->_firstArgName), _funcs(self->_currentFunctionName)
        {
            _args.emplace(args.empty() ? "" : args.front()->name);
            _funcs.emplace(name ? name->name : "");
        }
    };

public:
    Runtime::ObjectRef build(void);

/*** Language Structures ***/

protected:
    virtual void visitIf(const std::unique_ptr<AST::If> &node);
    virtual void visitFor(const std::unique_ptr<AST::For> &node);
    virtual void visitTry(const std::unique_ptr<AST::Try> &node);
    virtual void visitClass(const std::unique_ptr<AST::Class> &node);
    virtual void visitWhile(const std::unique_ptr<AST::While> &node);
    virtual void visitNative(const std::unique_ptr<AST::Native> &node);
    virtual void visitSwitch(const std::unique_ptr<AST::Switch> &node);
    virtual void visitFunction(const std::unique_ptr<AST::Function> &node);

private:
    /* generating code such as `SET_ATTR` */
    bool isInConstructor(void);
    void buildCompositeTarget(const std::unique_ptr<AST::Composite> &node);

protected:
    virtual void visitAssign(const std::unique_ptr<AST::Assign> &node);
    virtual void visitIncremental(const std::unique_ptr<AST::Incremental> &node);

/*** Misc. Statements ***/

protected:
    virtual void visitRaise(const std::unique_ptr<AST::Raise> &node);
    virtual void visitDelete(const std::unique_ptr<AST::Delete> &node);
    virtual void visitImport(const std::unique_ptr<AST::Import> &node);

/*** Control Flows ***/

protected:
    virtual void visitBreak(const std::unique_ptr<AST::Break> &node);
    virtual void visitReturn(const std::unique_ptr<AST::Return> &node);
    virtual void visitContinue(const std::unique_ptr<AST::Continue> &node);

/*** Object Modifiers ***/

protected:
    virtual void visitIndex(const std::unique_ptr<AST::Index> &node);
    virtual void visitInvoke(const std::unique_ptr<AST::Invoke> &node);
    virtual void visitAttribute(const std::unique_ptr<AST::Attribute> &node);

/*** Composite Literals ***/

protected:
    virtual void visitMap(const std::unique_ptr<AST::Map> &node);
    virtual void visitArray(const std::unique_ptr<AST::Array> &node);
    virtual void visitTuple(const std::unique_ptr<AST::Tuple> &node);

/*** Expressions ***/

protected:
    virtual void visitName(const std::unique_ptr<AST::Name> &node);
    virtual void visitUnpack(const std::unique_ptr<AST::Unpack> &node);
    virtual void visitLiteral(const std::unique_ptr<AST::Literal> &node);
    virtual void visitDecorator(const std::unique_ptr<AST::Decorator> &node);
    virtual void visitExpression(const std::unique_ptr<AST::Expression> &node);

};
}

#endif /* REDSCRIPT_COMPILER_CODEGENERATOR_H */
