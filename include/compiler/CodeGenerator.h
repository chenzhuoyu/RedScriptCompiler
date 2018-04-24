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
#include "runtime/RuntimeError.h"
#include "runtime/InternalError.h"

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
    struct GenerationFrame
    {
        typedef Runtime::Reference<Runtime::CodeObject> CodeReference;

    public:
        CodeType type;
        CodeReference code;
        std::stack<std::vector<uint32_t>> breakStack;
        std::stack<std::vector<uint32_t>> continueStack;

    public:
        GenerationFrame(CodeType type) :
            type(type),
            code(Runtime::Object::newObject<Runtime::CodeObject>()) {}

    };

private:
    std::stack<std::string> _firstArgName;
    std::stack<std::string> _currentFunctionName;

private:
    std::vector<GenerationFrame> _codeStack;
    std::unique_ptr<AST::CompoundStatement> _block;

public:
    virtual ~CodeGenerator() = default;
    explicit CodeGenerator(std::unique_ptr<AST::CompoundStatement> &&block) : _block(std::move(block)) {}

protected:
    inline auto &code(void) { return _codeStack.back().code; }
    inline auto &breakStack(void) { return _codeStack.back().breakStack; }
    inline auto &continueStack(void) { return _codeStack.back().continueStack; }

protected:
    inline std::vector<char> &buffer(void) { return code()->buffer(); }
    inline std::vector<std::string> &names(void) { return code()->names(); }
    inline std::vector<Runtime::ObjectRef> &consts(void) { return code()->consts(); }

protected:
    inline uint32_t addName(const std::string &name) { return code()->addName(name); }
    inline uint32_t addLocal(const std::string &name) { return code()->addLocal(name); }
    inline uint32_t addConst(Runtime::ObjectRef value) { return code()->addConst(value); }

protected:
    template <typename T> inline uint32_t emit(T &&t, Engine::OpCode op) { return code()->emit(t->row(), t->col(), op); }
    template <typename T> inline uint32_t emitJump(T &&t, Engine::OpCode op) { return code()->emitJump(t->row(), t->col(), op); }
    template <typename T> inline uint32_t emitOperand(T &&t, Engine::OpCode op, int32_t v) { return code()->emitOperand(t->row(), t->col(), op, v); }

protected:
    inline bool isLocal(const std::string &value) { return code()->isLocal(value); }
    inline void patchBranch(uint32_t offset, uint32_t address) { code()->patchBranch(offset, address); }

protected:
    inline uint32_t pc(void)
    {
        if (buffer().size() > UINT32_MAX)
            throw Runtime::RuntimeError("Code exceeds 4G limit");
        else
            return static_cast<uint32_t>(buffer().size());
    }

protected:
    class CodeScope final
    {
        bool _left;
        CodeGenerator *_self;

    public:
       ~CodeScope() { if (!_left) _self->_codeStack.pop_back(); }
        CodeScope(CodeGenerator *self, CodeType type) : _left(false), _self(self) { self->_codeStack.emplace_back(type); }

    public:
        Runtime::ObjectRef leave(void)
        {
            /* already left */
            if (_left)
                throw Runtime::InternalError("Outside of code scope");

            /* get the most recent code object */
            auto &code = _self->_codeStack;
            Runtime::ObjectRef ref = std::move(code.back().code);

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
            const std::unique_ptr<AST::Name> &name,
            const std::vector<std::unique_ptr<AST::Name>> &args
        ) : _args(self->_firstArgName), _funcs(self->_currentFunctionName)
        {
            _args.emplace(args.empty() ? "" : args.front()->name);
            _funcs.emplace(name ? name->name : "");
        }
    };

protected:
    class BreakableScope final
    {
        bool _left;
        CodeGenerator *_self;

    public:
       ~BreakableScope() { if (!_left) _self->breakStack().pop(); }
        BreakableScope(CodeGenerator *self) : _self(self), _left(false) { _self->breakStack().emplace(); }

    public:
        std::vector<uint32_t> leave(void)
        {
            /* already left */
            if (_left)
                throw Runtime::InternalError("Outside of breakable scope");

            /* get the most recent code object */
            auto &stack = _self->breakStack();
            std::vector<uint32_t> top = std::move(stack.top());

            /* pop from code stack */
            _left = true;
            return std::move(top);
        }
    };

protected:
    class ContinuableScope final
    {
        bool _left;
        CodeGenerator *_self;

    public:
       ~ContinuableScope() { if (!_left) _self->continueStack().pop(); }
        ContinuableScope(CodeGenerator *self) : _self(self), _left(false) { _self->continueStack().emplace(); }

    public:
        std::vector<uint32_t> leave(void)
        {
            /* already left */
            if (_left)
                throw Runtime::InternalError("Outside of continuable scope");

            /* get the most recent code object */
            auto &stack = _self->continueStack();
            std::vector<uint32_t> top = std::move(stack.top());

            /* pop from code stack */
            _left = true;
            return std::move(top);
        }
    };

public:
    Runtime::ObjectRef build(void);

/*** Language Structures ***/

protected:
    void buildClassObject(const std::unique_ptr<AST::Class> &node);
    void buildFunctionObject(const std::unique_ptr<AST::Function> &node);

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
