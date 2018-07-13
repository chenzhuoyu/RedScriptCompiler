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
#include "exceptions/RuntimeError.h"
#include "exceptions/InternalError.h"

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
    std::vector<GenerationFrame> _frames;
    std::unique_ptr<AST::CompoundStatement> _block;

public:
    virtual ~CodeGenerator() = default;
    explicit CodeGenerator(std::unique_ptr<AST::CompoundStatement> &&block) : _block(std::move(block)) {}

private:
    inline auto &code(void) { return _frames.back().code; }
    inline auto &breakStack(void) { return _frames.back().breakStack; }
    inline auto &continueStack(void) { return _frames.back().continueStack; }

private:
    inline std::vector<char> &buffer(void) { return code()->buffer(); }
    inline std::vector<std::string> &args(void) { return code()->args(); }
    inline std::vector<std::string> &names(void) { return code()->names(); }
    inline std::vector<Runtime::ObjectRef> &consts(void) { return code()->consts(); }

private:
    inline void setVargs(const std::string &vargs) { code()->setVargs(vargs); }
    inline void setKwargs(const std::string &kwargs) { code()->setKwargs(kwargs); }

private:
    inline uint32_t addName(const std::string &name) { return code()->addName(name); }
    inline uint32_t addLocal(const std::string &name) { return code()->addLocal(name); }
    inline uint32_t addConst(Runtime::ObjectRef value) { return code()->addConst(value); }

private:
    template <typename T> inline uint32_t emit(T &&t, Engine::OpCode op) { return code()->emit(t->row(), t->col(), op); }
    template <typename T> inline uint32_t emitJump(T &&t, Engine::OpCode op) { return code()->emitJump(t->row(), t->col(), op); }
    template <typename T> inline uint32_t emitOperand(T &&t, Engine::OpCode op, int32_t v) { return code()->emitOperand(t->row(), t->col(), op, v); }

private:
    inline bool isLocal(const std::string &value) { return code()->isLocal(value); }
    inline void patchBranch(uint32_t offset, uint32_t address) { code()->patchBranch(offset, address); }

private:
    inline uint32_t pc(void)
    {
        if (buffer().size() > UINT32_MAX)
            throw Exceptions::RuntimeError("Code exceeds 4G limit");
        else
            return static_cast<uint32_t>(buffer().size());
    }

private:
    class CodeFrame final
    {
        bool _left;
        CodeGenerator *_self;

    public:
       ~CodeFrame() { if (!_left) _self->_frames.pop_back(); }
        CodeFrame(CodeGenerator *self, CodeType type) : _left(false), _self(self) { self->_frames.emplace_back(type); }

    public:
        GenerationFrame::CodeReference leave(void)
        {
            /* already left */
            if (_left)
                throw Exceptions::InternalError("Outside of code scope");

            /* get the most recent code object */
            auto &code = _self->_frames;
            GenerationFrame::CodeReference ref = std::move(code.back().code);

            /* pop from code stack */
            _left = true;
            code.pop_back();
            return std::move(ref);
        }
    };

private:
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

private:
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
                throw Exceptions::InternalError("Outside of breakable scope");

            /* get the most recent code object */
            auto &stack = _self->breakStack();
            std::vector<uint32_t> top = std::move(stack.top());

            /* pop from code stack */
            _left = true;
            return std::move(top);
        }
    };

private:
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
                throw Exceptions::InternalError("Outside of continuable scope");

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

private:
    void buildClassObject(const std::unique_ptr<AST::Class> &node);
    void buildFunctionObject(const std::unique_ptr<AST::Function> &node);

private:
    virtual void visitIf(const std::unique_ptr<AST::If> &node) override;
    virtual void visitFor(const std::unique_ptr<AST::For> &node) override;
    virtual void visitTry(const std::unique_ptr<AST::Try> &node) override;
    virtual void visitClass(const std::unique_ptr<AST::Class> &node) override;
    virtual void visitWhile(const std::unique_ptr<AST::While> &node) override;
    virtual void visitNative(const std::unique_ptr<AST::Native> &node) override;
    virtual void visitSwitch(const std::unique_ptr<AST::Switch> &node) override;
    virtual void visitFunction(const std::unique_ptr<AST::Function> &node) override;

private:
    /* generating code such as `SET_ATTR` */
    bool isInConstructor(void);
    void buildCompositeTarget(const std::unique_ptr<AST::Composite> &node);

private:
    virtual void visitAssign(const std::unique_ptr<AST::Assign> &node) override;
    virtual void visitIncremental(const std::unique_ptr<AST::Incremental> &node) override;

/*** Misc. Statements ***/

private:
    virtual void visitRaise(const std::unique_ptr<AST::Raise> &node) override;
    virtual void visitDelete(const std::unique_ptr<AST::Delete> &node) override;
    virtual void visitImport(const std::unique_ptr<AST::Import> &node) override;

/*** Control Flows ***/

private:
    virtual void visitBreak(const std::unique_ptr<AST::Break> &node) override;
    virtual void visitReturn(const std::unique_ptr<AST::Return> &node) override;
    virtual void visitContinue(const std::unique_ptr<AST::Continue> &node) override;

/*** Object Modifiers ***/

private:
    virtual void visitIndex(const std::unique_ptr<AST::Index> &node) override;
    virtual void visitSlice(const std::unique_ptr<AST::Slice> &node) override;
    virtual void visitInvoke(const std::unique_ptr<AST::Invoke> &node) override;
    virtual void visitAttribute(const std::unique_ptr<AST::Attribute> &node) override;

/*** Composite Literals ***/

private:
    virtual void visitMap(const std::unique_ptr<AST::Map> &node) override;
    virtual void visitArray(const std::unique_ptr<AST::Array> &node) override;
    virtual void visitTuple(const std::unique_ptr<AST::Tuple> &node) override;

/*** Expressions ***/

private:
    virtual void visitName(const std::unique_ptr<AST::Name> &node) override;
    virtual void visitUnpack(const std::unique_ptr<AST::Unpack> &node) override;
    virtual void visitLiteral(const std::unique_ptr<AST::Literal> &node) override;
    virtual void visitDecorator(const std::unique_ptr<AST::Decorator> &node) override;
    virtual void visitExpression(const std::unique_ptr<AST::Expression> &node) override;

/*** Generic Statements ***/

private:
    virtual void visitStatement(const std::unique_ptr<AST::Statement> &node) override;

};
}

#endif /* REDSCRIPT_COMPILER_CODEGENERATOR_H */
