#ifndef REDSCRIPT_ENGINE_THREAD_H
#define REDSCRIPT_ENGINE_THREAD_H

#include <memory>
#include <vector>

#include "runtime/Object.h"
#include "runtime/CodeObject.h"
#include "runtime/ExceptionObject.h"

#include "engine/Bytecode.h"

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

    public:
        typedef const std::string &Name;
        typedef Runtime::Reference<Runtime::CodeObject> Code;

    private:
        Code _code;
        Name _name;
        std::pair<int, int> *_lines;

    public:
        Frame(Name name, Code &&code) : _name(name), _code(std::move(code))
        {
            _pc    = _code->buffer().data();
            _end   = _code->buffer().data() + _code->buffer().size();
            _begin = _code->buffer().data();
            _lines = _code->lineNums().data();
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
        inline size_t pc(void) const { return _pc - _begin; }
        inline const auto &name(void) const { return _name; }
        inline const auto &file(void) const { return _code->file(); }
        inline const auto &line(void) const { return _lines[_pc - _begin]; }

    public:
        inline void jumpBy(int32_t offset) __attribute__((always_inline))
        {
            /* calculate the jump target */
            const char *p = _pc + offset;
            const char *q = p - sizeof(uint32_t) - 1;

            /* check the jump address */
            if ((q < _begin) || (q >= _end))
                throw Runtime::Exceptions::InternalError("Jump outside of code");

            /* set the instruction pointer */
            _pc = q;
        }

    public:
        inline void jumpTo(size_t pc) __attribute__((always_inline))
        {
            /* check the jump address */
            if (_begin + pc >= _end)
                throw Runtime::Exceptions::InternalError("Jump outside of code");

            /* set the instruction pointer */
            _pc = _begin + pc;
        }

    public:
        inline OpCode nextOpCode(void) __attribute__((always_inline))
        {
            if (_pc >= _end)
                throw Runtime::Exceptions::InternalError("Unexpected termination of bytecode stream");
            else
                return static_cast<OpCode>(*_pc++);
        }

    public:
        inline uint32_t nextOperand(void) __attribute__((always_inline))
        {
            /* move to next instruction or operand */
            auto p = _pc;
            _pc += sizeof(uint32_t);

            /* check for program counter */
            if (_pc >= _end)
                throw Runtime::Exceptions::InternalError("Unexpected end of bytecode");
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
       ~FrameScope() { Thread::self()->framePop(); }
        FrameScope(Frame::Name name, Frame::Code code) :
            _frame(Thread::self()->framePush(name, std::move(code))) {}

    };

public:
    std::vector<std::unique_ptr<Frame>> frames;

public:
    Thread() { frames.reserve(MAX_RECURSION); }

public:
    inline void framePop(void) { frames.pop_back(); }
    inline Frame *framePush(Frame::Name name, Frame::Code &&code)
    {
        if (frames.size() >= MAX_RECURSION)
            throw Runtime::Exceptions::RuntimeError("Maximum recursion depth exceeded");
        else
            return frames.emplace_back(std::make_unique<Frame>(name, std::move(code))).get();
    }

public:
    static Thread *self(void)
    {
        static thread_local Thread thread;
        return &thread;
    }
};
}

#endif /* REDSCRIPT_ENGINE_THREAD_H */
