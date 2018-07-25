#ifndef REDSCRIPT_ENGINE_THREAD_H
#define REDSCRIPT_ENGINE_THREAD_H

#include <memory>
#include <vector>

#include "runtime/Object.h"
#include "runtime/CodeObject.h"

#include "engine/Bytecode.h"
#include "exceptions/RuntimeError.h"
#include "exceptions/InternalError.h"

namespace RedScript::Engine
{
struct Thread
{
    static const size_t MAX_RECURSION = 1024;

public:
    class Frame
    {
        const char *_pc;
        const char *_end;
        const char *_begin;

    private:
        std::pair<int, int> *_lines;
        Runtime::Reference<Runtime::CodeObject> _code;

    public:
        Frame(Runtime::Reference<Runtime::CodeObject> code) : _code(code)
        {
            _pc    = code->buffer().data();
            _end   = code->buffer().data() + code->buffer().size();
            _begin = code->buffer().data();
            _lines = code->lineNums().data();
        }

    public:
        ~Frame()
        {
            _pc = nullptr;
            _end = nullptr;
            _code = nullptr;
            _begin = nullptr;
            _lines = nullptr;
        }

    public:
        inline const char *pc(void) const { return _pc; }
        inline const auto &line(void) const { return _lines[_pc - _begin]; }

    public:
        inline void jump(int32_t offset)
        {
            /* calculate the jump target */
            const char *p = _pc + offset;
            const char *q = p - sizeof(uint32_t) - 1;

            /* check the jump address */
            if ((q < _begin) || (q >= _end))
                throw Exceptions::InternalError("Jump outside of code");

            /* set the instruction pointer */
            _pc = q;
        }

    public:
        inline OpCode next(void)
        {
            if (_pc >= _end)
                throw Exceptions::InternalError("Unexpected termination of bytecode stream");
            else
                return static_cast<OpCode>(*_pc++);
        }

    public:
        inline uint32_t nextOperand(void)
        {
            /* move to next instruction or operand */
            auto p = _pc;
            _pc += sizeof(uint32_t);

            /* check for program counter */
            if (_pc >= _end)
                throw Exceptions::InternalError("Unexpected end of bytecode");
            else
                return *reinterpret_cast<const uint32_t *>(p);
        }
    };

public:
    class FrameScope
    {
        Frame *_frame;

    public:
        inline Frame *frame(void) const { return _frame; }
        inline Frame *operator->(void) const { return _frame; }

    public:
        FrameScope(Runtime::Reference<Runtime::CodeObject> code)
        {
            /* check for maximum recursion depth */
            if (Thread::current()->frames.size() >= Thread::MAX_RECURSION)
                throw Exceptions::RuntimeError("Maximum recursion depth exceeded");

            /* create a new execution frame */
            Thread::current()->frames.emplace_back(std::make_unique<Frame>(code));
            _frame = Thread::current()->frames.back().get();
        }

    public:
        ~FrameScope()
        {
            _frame = nullptr;
            Thread::current()->frames.pop_back();
        }
    };

public:
    std::vector<std::unique_ptr<Frame>> frames;

public:
    Thread() { frames.reserve(MAX_RECURSION); }

public:
    static Thread *current(void)
    {
        static thread_local Thread thread;
        return &thread;
    }
};
}

#endif /* REDSCRIPT_ENGINE_THREAD_H */
