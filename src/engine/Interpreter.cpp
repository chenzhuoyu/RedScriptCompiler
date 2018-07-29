#include <stack>

#include "utils/Strings.h"
#include "engine/Thread.h"
#include "engine/Interpreter.h"

#include "runtime/MapObject.h"
#include "runtime/BoolObject.h"
#include "runtime/NullObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"
#include "runtime/FunctionObject.h"
#include "runtime/ExceptionObject.h"
#include "runtime/NativeClassObject.h"

namespace RedScript::Engine
{
namespace
{
static const size_t EF_INIT     = 0x00;
static const size_t EF_CAUGHT   = 0x01;
static const size_t EF_HANDLING = 0x02;
static const size_t EF_FINALLY  = 0x04;

struct Block
{
    size_t sp;
    size_t flags;
    size_t except;
    size_t finally;
    Runtime::Reference<Runtime::ExceptionObject> exception;
};
}

Runtime::ObjectRef Interpreter::tupleConcat(Runtime::ObjectRef a, Runtime::ObjectRef b)
{
    /* both are null */
    if (a.isNull() && b.isNull())
        return Runtime::TupleObject::newEmpty();

    /* a == null, b != null */
    if (a.isNull() && b.isNotNull())
        return b;

    /* b == null, a != null */
    if (b.isNull() && a.isNotNull())
        return a;

    /* check for left tuple, right tuple already checked before */
    if (a->isNotInstanceOf(Runtime::TupleTypeObject))
        throw Runtime::Exceptions::InternalError("Invalid left tuple object");

    /* calculate it's size, and allocate a new tuple */
    auto tupleA = a.as<Runtime::TupleObject>();
    auto tupleB = b.as<Runtime::TupleObject>();
    auto result = Runtime::TupleObject::fromSize(tupleA->size() + tupleB->size());

    /* copy all items from left tuple */
    for (size_t i = 0; i < tupleA->size(); i++)
        result->items()[i] = tupleA->items()[i];

    /* copy all items from right tuple */
    for (size_t i = 0; i < tupleB->size(); i++)
        result->items()[i + tupleA->size()] = tupleB->items()[i];

    /* move to prevent copy */
    return std::move(result);
}

Runtime::ObjectRef Interpreter::hashmapConcat(Runtime::ObjectRef a, Runtime::ObjectRef b)
{
    /* both are null */
    if (a.isNull() && b.isNull())
        return Runtime::MapObject::newOrdered();

    /* a == null, b != null */
    if (a.isNull() && b.isNotNull())
        return b;

    /* b == null, a != null */
    if (b.isNull() && a.isNotNull())
        return a;

    /* check for left map, right map already checked before */
    if (a->isNotInstanceOf(Runtime::MapTypeObject))
        throw Runtime::Exceptions::InternalError("Invalid left map object");

    /* convert to map types */
    auto mapA = a.as<Runtime::MapObject>();
    auto mapB = b.as<Runtime::MapObject>();
    auto result = Runtime::MapObject::newOrdered();

    /* all merge to result */
    mapA->enumerate([&](Runtime::ObjectRef key, Runtime::ObjectRef value){ result->insert(key, value); return true; });
    mapB->enumerate([&](Runtime::ObjectRef key, Runtime::ObjectRef value){ result->insert(key, value); return true; });

    /* move to prevent copy */
    return std::move(result);
}

std::unordered_map<std::string, Engine::ClosureRef> Interpreter::closureCreate(Runtime::Reference<Runtime::CodeObject> &code)
{
    /* locals map and result closure */
    std::unordered_map<std::string, uint32_t> &locals = _code->localMap();
    std::unordered_map<std::string, Engine::ClosureRef> closure;

    /* build class closure only from free variables to reduce closure size */
    for (const auto &item : code->freeVars())
    {
        /* find from local variables first */
        auto iter = locals.find(item);

        /* it's a local variable, wrap it as closure */
        if (iter != locals.end())
        {
            _closures[iter->second] = std::make_unique<Closure::Context>(&_locals, iter->second);
            closure.emplace(item, _closures[iter->second]->ref);
        }
        else
        {
            /* otherwise, find in closure map */
            auto cliter = _names.find(item);

            /* if found, inherit the closure */
            if (cliter != _names.end())
                closure.emplace(item, cliter->second);

            /* if not found, it should never be found again,
             * but we don't throw exceptions here, the opcode
             * LOAD_NAME will throw an exception for us when it
             * comes to resolving the name */
        }
    }

    /* move to prevent copy */
    return std::move(closure);
}

Runtime::ObjectRef Interpreter::eval(void)
{
    /* create a new execution frame */
    OpCode opcode;
    Thread::FrameScope frame(_name, _code);

    /* exception recovery stuff */
    std::stack<Block> blocks;
    Runtime::ObjectRef returnValue;

    /* loop until code ends */
    for (;;) try
    {
        /* main switch on opcodes */
        switch ((opcode = frame->nextOpCode()))
        {
            /* load null */
            case OpCode::LOAD_NULL:
            {
                _stack.emplace_back(Runtime::NullObject);
                break;
            }

            /* load true */
            case OpCode::LOAD_TRUE:
            {
                _stack.emplace_back(Runtime::TrueObject);
                break;
            }

            /* load false */
            case OpCode::LOAD_FALSE:
            {
                _stack.emplace_back(Runtime::FalseObject);
                break;
            }

            /* load constant */
            case OpCode::LOAD_CONST:
            {
                /* read constant ID */
                uint32_t id = frame->nextOperand();

                /* check constant ID */
                if (id >= _code->consts().size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Constant ID %u out of range", id));

                /* load the constant into stack */
                _stack.emplace_back(_code->consts()[id]);
                break;
            }

            /* load by name */
            case OpCode::LOAD_NAME:
            {
                /* read name ID */
                uint32_t id = frame->nextOperand();

                /* check name ID */
                if (id >= _code->names().size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                /* find name in locals */
                auto name = _code->names()[id];
                auto iter = _code->localMap().find(name);

                /* found, use the local value */
                if ((iter != _code->localMap().end()))
                {
                    /* must be assigned before using */
                    if (_locals[iter->second].isNull())
                    {
                        throw Runtime::Exceptions::NameError(
                            frame->line().first,
                            frame->line().second,
                            Utils::Strings::format("Variable \"%s\" referenced before assignment", name)
                        );
                    }

                    /* push onto the stack */
                    _stack.emplace_back(_locals[iter->second]);
                    break;
                }

                /* otherwise, find in closure name map */
                auto cliter = _names.find(name);

                /* found, use the closure value */
                if (cliter != _names.end())
                {
                    /* must be assigned before using */
                    if (cliter->second->get().isNull())
                    {
                        throw Runtime::Exceptions::NameError(
                            frame->line().first,
                            frame->line().second,
                            Utils::Strings::format("Variable \"%s\" referenced before assignment", name)
                        );
                    }

                    /* push onto the stack */
                    _stack.emplace_back(cliter->second->get());
                    break;
                }

                /* otherwise, it's an error */
                throw Runtime::Exceptions::NameError(
                    frame->line().first,
                    frame->line().second,
                    Utils::Strings::format("\"%s\" is not defined", name)
                );
            }

            /* load local variable */
            case OpCode::LOAD_LOCAL:
            {
                /* read local ID */
                uint32_t id = frame->nextOperand();

                /* check local ID */
                if (id >= _locals.size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Local ID %u out of range", id));

                /* should have initialized */
                if (_locals[id].isNull())
                {
                    throw Runtime::Exceptions::NameError(
                        frame->line().first,
                        frame->line().second,
                        Utils::Strings::format("Variable \"%s\" referenced before assignment", _code->locals()[id])
                    );
                }

                /* load the local into stack */
                _stack.emplace_back(_locals[id]);
                break;
            }

            /* set local variable */
            case OpCode::STOR_LOCAL:
            {
                /* read local ID */
                uint32_t id = frame->nextOperand();

                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* check local ID */
                if (id >= _locals.size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Local ID %u out of range", id));

                /* set the local from stack */
                _locals[id] = std::move(_stack.back());
                _stack.pop_back();
                break;
            }

            /* delete local variable by setting to NULL */
            case OpCode::DEL_LOCAL:
            {
                /* read local ID */
                uint32_t id = frame->nextOperand();

                /* check local ID */
                if (id >= _locals.size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Local ID %u out of range", id));

                /* should have initialized */
                if (_locals[id].isNull())
                {
                    throw Runtime::Exceptions::NameError(
                        frame->line().first,
                        frame->line().second,
                        Utils::Strings::format("Variable \"%s\" referenced before assignment", _code->locals()[id])
                    );
                }

                /* clear local reference */
                _locals[id] = nullptr;
                _closures[id] = nullptr;
                break;
            }

            /* define attributes */
            case OpCode::DEF_ATTR:
            {
                /* read local ID */
                uint32_t id = frame->nextOperand();

                /* check stack */
                if (_stack.size() < 2)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* check local ID */
                if (id >= _code->names().size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                /* pop top 2 elements */
                Runtime::ObjectRef a = std::move(*(_stack.end() - 2));
                Runtime::ObjectRef b = std::move(*(_stack.end() - 1));

                /* define attribute from stack */
                _stack.resize(_stack.size() - 2);
                b->type()->objectDefineAttr(b, _code->names()[id], a);
                break;
            }

            /* get attributes */
            case OpCode::GET_ATTR:
            {
                /* read local ID */
                uint32_t id = frame->nextOperand();

                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* check local ID */
                if (id >= _code->names().size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                /* replace stack top with new object */
                _stack.back() = _stack.back()->type()->objectGetAttr(_stack.back(), _code->names()[id]);
                break;
            }

            /* set attributes, error if not exists */
            case OpCode::SET_ATTR:
            {
                /* read local ID */
                uint32_t id = frame->nextOperand();

                /* check stack */
                if (_stack.size() < 2)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* check local ID */
                if (id >= _code->names().size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                /* pop top 2 elements */
                Runtime::ObjectRef a = std::move(*(_stack.end() - 2));
                Runtime::ObjectRef b = std::move(*(_stack.end() - 1));

                /* define attribute from stack */
                _stack.resize(_stack.size() - 2);
                b->type()->objectSetAttr(b, _code->names()[id], a);
                break;
            }

            /* remove attributes */
            case OpCode::DEL_ATTR:
            {
                /* read local ID */
                uint32_t id = frame->nextOperand();

                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* check local ID */
                if (id >= _code->names().size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                /* remove the attribute */
                _stack.back()->type()->objectDelAttr(_stack.back(), _code->names()[id]);
                _stack.pop_back();
                break;
            }

            /* get by index */
            case OpCode::GET_ITEM:
            {
                /* check stack */
                if (_stack.size() < 2)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* pop top 2 elements */
                Runtime::ObjectRef a = std::move(*(_stack.end() - 2));
                Runtime::ObjectRef b = std::move(*(_stack.end() - 1));

                /* replace stack top with new object */
                _stack.pop_back();
                _stack.back() = a->type()->sequenceGetItem(a, b);
                break;
            }

            /* set by index */
            case OpCode::SET_ITEM:
            {
                /* check stack */
                if (_stack.size() < 3)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* pop top 3 elements */
                Runtime::ObjectRef a = std::move(*(_stack.end() - 3));
                Runtime::ObjectRef b = std::move(*(_stack.end() - 2));
                Runtime::ObjectRef c = std::move(*(_stack.end() - 1));

                /* set the index from stack */
                _stack.resize(_stack.size() - 3);
                b->type()->sequenceSetItem(b, c, a);
                break;
            }

            /* remove by index */
            case OpCode::DEL_ITEM:
            {
                /* check stack */
                if (_stack.size() < 2)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* pop top 2 elements */
                Runtime::ObjectRef a = std::move(*(_stack.end() - 2));
                Runtime::ObjectRef b = std::move(*(_stack.end() - 1));

                /* delete index from stack */
                _stack.resize(_stack.size() - 2);
                a->type()->sequenceDelItem(a, b);
                break;
            }

            /* sequence slicing */
            case OpCode::GET_SLICE:
            case OpCode::SET_SLICE:
            case OpCode::DEL_SLICE:
            {
                /* slicing flags */
                uint32_t flags = frame->nextOperand();
                Runtime::ObjectRef end = nullptr;
                Runtime::ObjectRef step = nullptr;
                Runtime::ObjectRef begin = nullptr;

                /* read stepping if any */
                if (flags & Engine::SL_STEP)
                {
                    /* check stack */
                    if (_stack.empty())
                        throw Runtime::Exceptions::InternalError("Stack is empty");

                    /* pop from stack */
                    step = std::move(_stack.back());
                    _stack.pop_back();
                }

                /* read ending if any */
                if (flags & Engine::SL_END)
                {
                    /* check stack */
                    if (_stack.empty())
                        throw Runtime::Exceptions::InternalError("Stack is empty");

                    /* pop from stack */
                    end = std::move(_stack.back());
                    _stack.pop_back();
                }

                /* read beginning if any */
                if (flags & Engine::SL_BEGIN)
                {
                    /* check stack */
                    if (_stack.empty())
                        throw Runtime::Exceptions::InternalError("Stack is empty");

                    /* pop from stack */
                    begin = std::move(_stack.back());
                    _stack.pop_back();
                }

                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* pop slicing target object from stack */
                Runtime::ObjectRef obj = std::move(_stack.back());
                _stack.pop_back();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"

                switch (opcode)
                {
                    /* get by slicing */
                    case OpCode::GET_SLICE:
                    {
                        _stack.emplace_back(obj->type()->sequenceGetSlice(obj, begin, end, step));
                        break;
                    }

                    /* set by slicing */
                    case OpCode::SET_SLICE:
                    {
                        obj->type()->sequenceSetSlice(obj, begin, end, step, std::move(_stack.back()));
                        _stack.pop_back();
                        break;
                    }

                    /* delete by slicing */
                    case OpCode::DEL_SLICE:
                    {
                        obj->type()->sequenceDelSlice(obj, begin, end, step);
                        break;
                    }
                }

#pragma clang diagnostic pop

                break;
            }

            /* return stack top */
            case OpCode::POP_RETURN:
            {
                /* check for stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* pop the stack top, that's the return value */
                returnValue = std::move(_stack.back());
                _stack.pop_back();

                /* should have no items left */
                if (!(_stack.empty()))
                    throw Runtime::Exceptions::InternalError("Stack not empty when return");

                /* if we have exception rescure blocks remaining,
                 * we should execute their "finally" blocks before returning */
                if (blocks.empty())
                    return std::move(returnValue);

                /* jump to the nearest block */
                frame->jumpTo(blocks.top().finally);
                break;
            }

            /* invoke as function */
            case OpCode::CALL_FUNCTION:
            {
                /* invocation flags */
                uint32_t flags = frame->nextOperand();
                Runtime::ObjectRef vargs = nullptr;
                Runtime::ObjectRef kwargs = nullptr;

                /* decorator invocation */
                if (flags & FI_DECORATOR)
                {
                    /* should be the only flag */
                    if (flags != FI_DECORATOR)
                        throw Runtime::Exceptions::InternalError(Utils::Strings::format("Invalid invocation flags 0x%.8x", flags));

                    /* check for stack */
                    if (_stack.empty())
                        throw Runtime::Exceptions::InternalError("Stack is empty");

                    /* build a tuple from stack top */
                    vargs = Runtime::TupleObject::fromObjects(_stack.back());
                    kwargs = Runtime::MapObject::newOrdered();
                    _stack.pop_back();
                }

                /* normal invocation */
                else
                {
                    Runtime::ObjectRef args = nullptr;
                    Runtime::ObjectRef named = nullptr;

                    /* have keyword arguments */
                    if (flags & FI_KWARGS)
                    {
                        /* check for stack */
                        if (_stack.empty())
                            throw Runtime::Exceptions::InternalError("Stack is empty");

                        /* pop keyword arguments from stack */
                        kwargs = std::move(_stack.back());
                        _stack.pop_back();

                        /* must be a map */
                        if (kwargs->isNotInstanceOf(Runtime::MapTypeObject))
                            throw Runtime::Exceptions::TypeError("Keyword argument pack must be a map");
                    }

                    /* have variadic arguments */
                    if (flags & FI_VARGS)
                    {
                        /* check for stack */
                        if (_stack.empty())
                            throw Runtime::Exceptions::InternalError("Stack is empty");

                        /* pop variadic arguments from stack */
                        vargs = std::move(_stack.back());
                        _stack.pop_back();

                        /* must be a tuple */
                        if (vargs->isNotInstanceOf(Runtime::TupleTypeObject))
                            throw Runtime::Exceptions::TypeError("Variadic argument pack must be a tuple");
                    }

                    /* have named arguments */
                    if (flags & FI_NAMED)
                    {
                        /* check for stack */
                        if (_stack.empty())
                            throw Runtime::Exceptions::InternalError("Stack is empty");

                        /* pop named arguments from stack */
                        named = std::move(_stack.back());
                        _stack.pop_back();
                    }

                    /* have arguments */
                    if (flags & FI_ARGS)
                    {
                        /* check for stack */
                        if (_stack.empty())
                            throw Runtime::Exceptions::InternalError("Stack is empty");

                        /* pop normal arguments from stack */
                        args = std::move(_stack.back());
                        _stack.pop_back();
                    }

                    /* merge with other arguments */
                    vargs = tupleConcat(args, vargs);
                    kwargs = hashmapConcat(named, kwargs);
                }

                /* check for stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* tuple type check */
                if (vargs->isNotInstanceOf(Runtime::TupleTypeObject))
                    throw Runtime::Exceptions::InternalError("Invalid function call: vargs");

                /* map type check */
                if (kwargs->isNotInstanceOf(Runtime::MapTypeObject))
                    throw Runtime::Exceptions::InternalError("Invalid function call: kwargs");

                /* pop callable object from stack */
                Runtime::ObjectRef obj = std::move(_stack.back());
                Runtime::Reference<Runtime::MapObject> namedArgs = kwargs.as<Runtime::MapObject>();
                Runtime::Reference<Runtime::TupleObject> indexArgs = vargs.as<Runtime::TupleObject>();

                /* invoke object as function */
                _stack.back() = obj->type()->objectInvoke(obj, std::move(indexArgs), std::move(namedArgs));
                break;
            }

            /* duplicate stack top */
            case OpCode::DUP:
            {
                /* check for stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* pushing the top again */
                _stack.emplace_back(_stack.back());
                break;
            }

            /* duplicate top 2 elements of stack */
            case OpCode::DUP2:
            {
                /* check for stack */
                if (_stack.size() < 2)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* pushing the top two elements */
                _stack.emplace_back(*(_stack.end() - 2));
                _stack.emplace_back(*(_stack.end() - 2));
                break;
            }

            /* discard stack top */
            case OpCode::DROP:
            {
                /* check for stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* discard stack top */
                _stack.pop_back();
                break;
            }

            /* swap top 2 items */
            case OpCode::SWAP:
            {
                /* check for stack */
                if (_stack.size() < 2)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* get the top two elements */
                Runtime::ObjectRef a = std::move(*(_stack.end() - 1));
                Runtime::ObjectRef b = std::move(*(_stack.end() - 2));

                /* swap the order */
                *(_stack.end() - 1) = std::move(b);
                *(_stack.end() - 2) = std::move(a);
                break;
            }

            /* rotate top 3 items */
            case OpCode::ROTATE:
            {
                /* check for stack */
                if (_stack.size() < 3)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* get the top two elements */
                Runtime::ObjectRef a = std::move(*(_stack.end() - 1));
                Runtime::ObjectRef b = std::move(*(_stack.end() - 2));
                Runtime::ObjectRef c = std::move(*(_stack.end() - 3));

                /* swap the order */
                *(_stack.end() - 1) = std::move(b);
                *(_stack.end() - 2) = std::move(c);
                *(_stack.end() - 3) = std::move(a);
                break;
            }

            /* unary operators */
            case OpCode::POS:
            case OpCode::NEG:
            case OpCode::BIT_NOT:
            case OpCode::BOOL_NOT:
            case OpCode::MAKE_ITER:
            {
                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* retrive stack top */
                Runtime::ObjectRef r;
                Runtime::ObjectRef a = std::move(_stack.back());

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"

                /* dispatch operators */
                switch (opcode)
                {
                    case OpCode::POS       : r = a->type()->numericPos(a); break;
                    case OpCode::NEG       : r = a->type()->numericNeg(a); break;
                    case OpCode::BIT_NOT   : r = a->type()->numericNot(a); break;
                    case OpCode::BOOL_NOT  : r = a->type()->boolNot(a); break;
                    case OpCode::MAKE_ITER : r = a->type()->iterableIter(a); break;
                }

#pragma clang diagnostic pop

                /* replace the stack top */
                _stack.back() = std::move(r);
                break;
            }

            /* binary operators */
            case OpCode::ADD:
            case OpCode::SUB:
            case OpCode::MUL:
            case OpCode::DIV:
            case OpCode::MOD:
            case OpCode::POWER:
            case OpCode::BIT_OR:
            case OpCode::BIT_AND:
            case OpCode::BIT_XOR:
            case OpCode::LSHIFT:
            case OpCode::RSHIFT:
            case OpCode::INP_ADD:
            case OpCode::INP_SUB:
            case OpCode::INP_MUL:
            case OpCode::INP_DIV:
            case OpCode::INP_MOD:
            case OpCode::INP_POWER:
            case OpCode::INP_BIT_OR:
            case OpCode::INP_BIT_AND:
            case OpCode::INP_BIT_XOR:
            case OpCode::INP_LSHIFT:
            case OpCode::INP_RSHIFT:
            case OpCode::BOOL_OR:
            case OpCode::BOOL_AND:
            case OpCode::EQ:
            case OpCode::LT:
            case OpCode::GT:
            case OpCode::NEQ:
            case OpCode::LEQ:
            case OpCode::GEQ:
            case OpCode::IS:
            case OpCode::IN:
            {
                /* check stack */
                if (_stack.size() < 2)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* pop top 2 elements */
                Runtime::ObjectRef r;
                Runtime::ObjectRef a = std::move(*(_stack.end() - 2));
                Runtime::ObjectRef b = std::move(*(_stack.end() - 1));
                _stack.pop_back();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"

                /* dispatch operators */
                switch (opcode)
                {
                    /* arithmetic operators */
                    case OpCode::ADD         : r = a->type()->numericAdd(a, b); break;
                    case OpCode::SUB         : r = a->type()->numericSub(a, b); break;
                    case OpCode::MUL         : r = a->type()->numericMul(a, b); break;
                    case OpCode::DIV         : r = a->type()->numericDiv(a, b); break;
                    case OpCode::MOD         : r = a->type()->numericMod(a, b); break;
                    case OpCode::POWER       : r = a->type()->numericPower(a, b); break;

                    /* bitwise operators */
                    case OpCode::BIT_OR      : r = a->type()->numericOr(a, b); break;
                    case OpCode::BIT_AND     : r = a->type()->numericAnd(a, b); break;
                    case OpCode::BIT_XOR     : r = a->type()->numericXor(a, b); break;

                    /* bit shifting operators */
                    case OpCode::LSHIFT      : r = a->type()->numericLShift(a, b); break;
                    case OpCode::RSHIFT      : r = a->type()->numericRShift(a, b); break;

                    /* in-place arithmetic operators */
                    case OpCode::INP_ADD     : r = a->type()->numericIncAdd(a, b); break;
                    case OpCode::INP_SUB     : r = a->type()->numericIncSub(a, b); break;
                    case OpCode::INP_MUL     : r = a->type()->numericIncMul(a, b); break;
                    case OpCode::INP_DIV     : r = a->type()->numericIncDiv(a, b); break;
                    case OpCode::INP_MOD     : r = a->type()->numericIncMod(a, b); break;
                    case OpCode::INP_POWER   : r = a->type()->numericIncPower(a, b); break;

                    /* in-place bitwise operators */
                    case OpCode::INP_BIT_OR  : r = a->type()->numericIncOr(a, b); break;
                    case OpCode::INP_BIT_AND : r = a->type()->numericIncAnd(a, b); break;
                    case OpCode::INP_BIT_XOR : r = a->type()->numericIncXor(a, b); break;

                    /* in-place bit shifting operators */
                    case OpCode::INP_LSHIFT  : r = a->type()->numericIncLShift(a, b); break;
                    case OpCode::INP_RSHIFT  : r = a->type()->numericIncRShift(a, b); break;

                    /* boolean logic operators */
                    case OpCode::BOOL_OR     : r = a->type()->boolOr(a, b); break;
                    case OpCode::BOOL_AND    : r = a->type()->boolAnd(a, b); break;

                    /* comparison operators */
                    case OpCode::EQ          : r = a->type()->comparableEq(a, b); break;
                    case OpCode::LT          : r = a->type()->comparableLt(a, b); break;
                    case OpCode::GT          : r = a->type()->comparableGt(a, b); break;
                    case OpCode::NEQ         : r = a->type()->comparableNeq(a, b); break;
                    case OpCode::LEQ         : r = a->type()->comparableLeq(a, b); break;
                    case OpCode::GEQ         : r = a->type()->comparableGeq(a, b); break;

                    /* special operator "is": absolute equals */
                    case OpCode::IS:
                    {
                        r = Runtime::BoolObject::fromBool(a.isIdenticalWith(b));
                        break;
                    }

                    /* special operator "in": "a in b" means "b.__contains__(a)" */
                    case OpCode::IN:
                    {
                        r = b->type()->comparableContains(b, a);
                        break;
                    }
                }

#pragma clang diagnostic pop

                /* replace the stack top */
                _stack.back() = std::move(r);
                break;
            }

            /* unconditional branch */
            case OpCode::BR:
            {
                frame->jumpBy(frame->nextOperand());
                break;
            }

            /* conditional branches */
            case OpCode::BRP_TRUE:
            case OpCode::BRP_FALSE:
            case OpCode::BRCP_TRUE:
            case OpCode::BRCP_FALSE:
            case OpCode::BRNP_TRUE:
            case OpCode::BRNP_FALSE:
            {
                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* evaluate condition */
                auto off = frame->nextOperand();
                bool isTrue = _stack.back()->isTrue();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"

                /* adjust stack */
                switch (opcode)
                {
                    /* always pop */
                    case OpCode::BRP_TRUE:
                    case OpCode::BRP_FALSE:
                    {
                        _stack.pop_back();
                        break;
                    }

                    /* pop if not true */
                    case OpCode::BRCP_TRUE:
                    {
                        if (!isTrue) _stack.pop_back();
                        break;
                    }

                    /* pop if not false */
                    case OpCode::BRCP_FALSE:
                    {
                        if (isTrue) _stack.pop_back();
                        break;
                    }

                    /* don't pop */
                    case OpCode::BRNP_TRUE:
                    case OpCode::BRNP_FALSE:
                        break;
                }

                /* set instruction pointers */
                switch (opcode)
                {
                    /* branch if true */
                    case OpCode::BRP_TRUE:
                    case OpCode::BRCP_TRUE:
                    case OpCode::BRNP_TRUE:
                    {
                        if (isTrue) frame->jumpBy(off);
                        break;
                    }

                    /* branch if false */
                    case OpCode::BRP_FALSE:
                    case OpCode::BRCP_FALSE:
                    case OpCode::BRNP_FALSE:
                    {
                        if (!isTrue) frame->jumpBy(off);
                        break;
                    }
                }

#pragma clang diagnostic pop

                break;
            }

            /* throw exception */
            case OpCode::RAISE:
            {
                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* pop the exception object from stack */
                auto value = std::move(_stack.back());
                _stack.pop_back();

                /* check for exception type, use the root class type
                 * checker to prevent user from overriding "__is_subclass_of__" */
                if (!(Runtime::TypeObject->nativeObjectIsSubclassOf(value->type(), Runtime::BaseExceptionTypeObject)))
                {
                    throw Runtime::Exceptions::TypeError(Utils::Strings::format(
                        "Exceptions must be derived from BaseException, not \"%s\"",
                        value->type()->name()
                    ));
                }

                /* throw the exception, this should never returns */
                throw value.as<Runtime::ExceptionObject>().retain();
            }

            /* exception matching */
            case OpCode::EXC_MATCH:
            case OpCode::EXC_STORE:
            {
                /* local ID and jump offset */
                uint32_t index = 0;
                uint32_t offset = frame->nextOperand();

                /* extract the local ID as needed */
                if (opcode == OpCode::EXC_STORE)
                    if ((index = frame->nextOperand()) >= _locals.size())
                        throw Runtime::Exceptions::InternalError(Utils::Strings::format("Local ID %u out of range", index));

                /* check for exception blocks */
                if (blocks.empty())
                    throw Runtime::Exceptions::InternalError("No blocks");

                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* pop the exception type from stack */
                auto type = std::move(_stack.back());
                _stack.pop_back();

                /* not even a type */
                if (type->isNotInstanceOf(Runtime::TypeObject))
                {
                    frame->jumpBy(offset);
                    break;
                }

                /* exception type and value */
                auto etype = type.as<Runtime::Type>();
                auto &exception = blocks.top().exception;

                /* check for exception type, use the root class type
                 * checker to prevent user from overriding "__is_subclass_of__" */
                if (!(Runtime::TypeObject->nativeObjectIsSubclassOf(exception->type(), etype)))
                {
                    frame->jumpBy(offset);
                    break;
                }

                /* save the exception object as needed */
                if (opcode == OpCode::EXC_STORE)
                    _locals[index] = exception;

                /* set the "matched" flag */
                blocks.top().flags |= EF_CAUGHT;
                break;
            }

            /* setup exception handling block */
            case OpCode::SETUP_BLOCK:
            {
                /* exception block ID */
                Block block;
                size_t start = frame->pc();
                uint32_t except = frame->nextOperand();
                uint32_t finally = frame->nextOperand();

                /* fill the block info */
                block.sp = _stack.size();
                block.flags = EF_INIT;
                block.except = start + except - 1;
                block.finally = start + finally - 1;

                /* push to block stack */
                blocks.push(std::move(block));
                break;
            }

            /* end of the exception handling block */
            case OpCode::END_EXCEPT:
            {
                /* check for exception blocks */
                if (blocks.empty())
                    throw Runtime::Exceptions::InternalError("No blocks");

                /* mark "finally" begins */
                blocks.top().flags |= EF_FINALLY;
                break;
            }

            /* end of the finally block */
            case OpCode::END_FINALLY:
            {
                /* check for exception blocks */
                if (blocks.empty())
                    throw Runtime::Exceptions::InternalError("No blocks");

                /* pop the block from stack */
                Block block = std::move(blocks.top());
                blocks.pop();

                /* stack should balance */
                if (_stack.size() != block.sp)
                {
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format(
                        "Stack unbalanced: %zu -> %zu",
                        block.sp,
                        _stack.size()
                    ));
                }

                /* exceptions occured but not handled, rethrow the exception */
                if (block.flags && !(block.flags & EF_CAUGHT))
                    throw block.exception;

                /* we got an return value, flush remaining blocks if any */
                if (returnValue.isNotNull())
                {
                    /* no other blocks remaining */
                    if (blocks.empty())
                        return std::move(returnValue);

                    /* otherwise, jump to it's finally section to
                     * execute the remaining "finally" instructions */
                    frame->jumpTo(blocks.top().finally);
                }

                break;
            }

            /* advance iterator */
            case OpCode::ITER_NEXT:
            {
                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* jump address when iterator drained */
                auto off = frame->nextOperand();
                auto &iter = _stack.back();

                /* try push new object on stack top, don't destroy the iterator */
                try
                {
                    _stack.emplace_back(iter->type()->iterableNext(iter));
                    break;
                }

                /* iterator drained, drop the iterator, and jump to address */
                catch (const Runtime::Exceptions::StopIteration &)
                {
                    frame->jumpBy(off);
                    _stack.pop_back();
                    break;
                }
            }

            /* expand sequence in reverse order */
            case OpCode::EXPAND_SEQ:
            {
                /* expected sequence size */
                uint32_t count = frame->nextOperand();

                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* pop the stack top */
                Runtime::ObjectRef top = std::move(_stack.back());
                _stack.pop_back();

                /* shortcut for arrays and tuples */
                if (top->isInstanceOf(Runtime::ArrayTypeObject) ||
                    top->isInstanceOf(Runtime::TupleTypeObject))
                {
                    size_t size;
                    Runtime::ObjectRef *data;

                    /* extract items and count */
                    if (top->isInstanceOf(Runtime::TupleTypeObject))
                    {
                        size = top.as<Runtime::TupleObject>()->size();
                        data = top.as<Runtime::TupleObject>()->items();
                    }
                    else
                    {
                        size = top.as<Runtime::ArrayObject>()->size();
                        data = top.as<Runtime::ArrayObject>()->items().data();
                    }

                    /* check item count */
                    if (size != count)
                    {
                        throw Runtime::Exceptions::ValueError(Utils::Strings::format(
                            "Needs exact %zu items to unpack, but got %zu",
                            count - size
                        ));
                    }

                    /* push back to the stack in reverse order */
                    for (size_t i = size; i > 0; i--)
                        _stack.emplace_back(data[i - 1]);
                }
                else
                {
                    /* convert to iterator */
                    size_t i = 0;
                    Runtime::ObjectRef iter = top->type()->iterableIter(top);
                    std::vector<Runtime::ObjectRef> items(count);

                    /* get all items from iterator */
                    try
                    {
                        while (i < count)
                        {
                            items[i] = iter->type()->iterableNext(iter);
                            i++;
                        }
                    }

                    /* iterator drained in the middle of expanding */
                    catch (const Runtime::Exceptions::StopIteration &)
                    {
                        throw Runtime::Exceptions::ValueError(Utils::Strings::format(
                            "Needs %zu more items to unpack",
                            count - i
                        ));
                    }

                    /* and the iterator should have exact `count` items */
                    try
                    {
                        iter->type()->iterableNext(iter);
                        throw Runtime::Exceptions::ValueError("Too many items to unpack");
                    }

                    /* `StopIteration` is expected */
                    catch (const Runtime::Exceptions::StopIteration &)
                    {
                        /* should have no more items */
                        /* this is expected, so ignore this exception */
                    }

                    /* push back to the stack in reverse order */
                    _stack.insert(_stack.end(), items.rbegin(), items.rend());
                }

                break;
            }

            /* import module */
            case OpCode::IMPORT_ALIAS:
            {
                // TODO: implement these
                throw Runtime::Exceptions::InternalError("not implemented: IMPORT_ALIAS");
            }

            /* build a map object */
            case OpCode::MAKE_MAP:
            {
                /* item count */
                auto map = Runtime::MapObject::newOrdered();
                uint32_t count = frame->nextOperand();

                /* check stack size */
                if (_stack.size() < count * 2)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* build the map */
                for (auto it = _stack.end() - count * 2; it != _stack.end(); it += 2)
                    map->insert(std::move(*it), std::move(*(it + 1)));

                /* adjust the stack */
                _stack.resize(_stack.size() - count * 2);
                _stack.emplace_back(map);
                break;
            }

            /* build an array object */
            case OpCode::MAKE_ARRAY:
            {
                /* create array object */
                auto size = frame->nextOperand();
                auto iter = _stack.rbegin();
                auto array = Runtime::Object::newObject<Runtime::ArrayObject>(size);

                /* check stack */
                if (_stack.size() < size)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* extract each item, in reverse order */
                for (ssize_t i = size; i > 0; i--)
                    array->items()[i - 1] = std::move(*iter++);

                /* push result into stack */
                _stack.resize(_stack.size() - size);
                _stack.emplace_back(std::move(array));
                break;
            }

            /* build a tuple object */
            case OpCode::MAKE_TUPLE:
            {
                /* create tuple object */
                auto size = frame->nextOperand();
                auto iter = _stack.rbegin();
                auto tuple = Runtime::TupleObject::fromSize(size);

                /* check stack */
                if (_stack.size() < size)
                    throw Runtime::Exceptions::InternalError("Stack underflow");

                /* extract each item, in reverse order */
                for (ssize_t i = size; i > 0; i--)
                    tuple->items()[i - 1] = std::move(*iter++);

                /* push result into stack */
                _stack.resize(_stack.size() - size);
                _stack.emplace_back(std::move(tuple));
                break;
            }

            /* build a function object */
            case OpCode::MAKE_FUNCTION:
            {
                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* code object ID and defaults pack */
                auto nid = frame->nextOperand();
                auto cid = frame->nextOperand();
                auto def = std::move(_stack.back());

                /* check name ID */
                if (nid >= _code->names().size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", cid));

                /* check code ID */
                if (cid >= _code->consts().size())
                    throw Runtime::Exceptions::InternalError(Utils::Strings::format("Code ID %u out of range", cid));

                /* check for tuple object */
                if (def->isNotInstanceOf(Runtime::TupleTypeObject))
                    throw Runtime::Exceptions::InternalError("Invalid tuple object");

                /* get function code and name */
                auto &name = _code->names()[nid];
                auto &code = _code->consts()[cid];

                /* check for code object */
                if (code->isNotInstanceOf(Runtime::CodeTypeObject))
                    throw Runtime::Exceptions::InternalError("Invalid code object");

                /* build execution closure */
                auto funcDef = def.as<Runtime::TupleObject>();
                auto funcCode = code.as<Runtime::CodeObject>();
                auto funcClosure = closureCreate(funcCode);

                /* create function object, and put on stack */
                _stack.back() = Runtime::Object::newObject<Runtime::FunctionObject>(name, funcCode, funcDef, funcClosure);
                break;
            }

            /* build a class object */
            case OpCode::MAKE_CLASS:
            {
                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* extract constant ID */
                auto nid = frame->nextOperand();
                auto cid = frame->nextOperand();
                auto super = std::move(_stack.back());

                /* check name ID range */
                if (nid >= _code->names().size())
                    throw Runtime::Exceptions::InternalError("Name ID out of range");

                /* check code ID range */
                if (cid >= _code->consts().size())
                    throw Runtime::Exceptions::InternalError("Code ID out of range");

                /* super class must be a type */
                if (super->isNotInstanceOf(Runtime::TypeObject))
                {
                    throw Runtime::Exceptions::TypeError(Utils::Strings::format(
                        "Super class must be a type, not \"%s\"",
                        super->type()->name()
                    ));
                }

                /* get function code and name */
                auto &name = _code->names()[nid];
                auto &code = _code->consts()[cid];

                /* check for code object */
                if (code->isNotInstanceOf(Runtime::CodeTypeObject))
                    throw Runtime::Exceptions::InternalError("Invalid code object");

                /* build execution closure */
                auto classCode = code.as<Runtime::CodeObject>();
                auto classSuper = super.as<Runtime::Type>();
                auto classClosure = closureCreate(classCode);

                /* create a new interpreter for class body,
                 * and a static map object for class attribues */
                Interpreter vm(name, classCode, classClosure);
                Runtime::Reference<Runtime::MapObject> dict = Runtime::MapObject::newOrdered();

                /* execute the class body */
                if (vm.eval().isNotIdenticalWith(Runtime::NullObject))
                    throw Runtime::Exceptions::InternalError("Invalid class body");

                /* enumerate every locals, but exclude not initialized one */
                for (auto &item : classCode->localMap())
                    if (vm._locals[item.second].isNotNull())
                        dict->insert(Runtime::StringObject::fromStringInterned(item.first), vm._locals[item.second]);

                /* create a class object, and put on stack */
                _stack.back() = Runtime::Type::create(name, std::move(dict), std::move(classSuper));
                break;
            }

            /* build a native class object */
            case OpCode::MAKE_NATIVE:
            {
                /* extract constant ID */
                uint32_t id = frame->nextOperand();

                /* check stack */
                if (_stack.empty())
                    throw Runtime::Exceptions::InternalError("Stack is empty");

                /* check operand range */
                if (id >= _code->consts().size())
                    throw Runtime::Exceptions::InternalError("Constant ID out of range");

                /* load options from stack */
                Runtime::ObjectRef source = _code->consts()[id];
                Runtime::ObjectRef options = std::move(_stack.back());

                /* must be a map */
                if (source->type() != Runtime::StringTypeObject)
                    throw Runtime::Exceptions::InternalError("Invalid source string");

                /* must be a map */
                if (options->type() != Runtime::MapTypeObject)
                    throw Runtime::Exceptions::InternalError("Invalid options map");

                /* replace stack top with native class object */
                _stack.back() = Runtime::Object::newObject<Runtime::NativeClassObject>(
                    source.as<Runtime::StringObject>()->value(),
                    options.as<Runtime::MapObject>()
                );

                break;
            }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "DuplicateSwitchCase"

            /* even though we have covered all cases in `OpCode`, we still
             * need this, since the opcode is direcly casted from a byte,
             * which might be an illegal value */
            default:
            {
                throw Runtime::Exceptions::InternalError(Utils::Strings::format(
                    "Invalid op-code 0x%.2x",
                    static_cast<uint8_t>(opcode)
                ));
            }

#pragma clang diagnostic pop

        }
    }

    /* exceptions occured when executing, try exception recovery */
    catch (const Runtime::Reference<Runtime::ExceptionObject> &e)
    {
        /* no exception recovery blocks available,
         * propagate the exception to parent scope */
        if (blocks.empty())
            throw;

        /* get the last block */
        Block *block = &(blocks.top());
        Runtime::Reference<Runtime::ExceptionObject> exc = e;

        /* process every block */
        for (;;)
        {
            /* check the stack size */
            if (_stack.size() < block->sp)
                throw Runtime::Exceptions::InternalError("Stack underflow in exception recovery block");

            /* chain the exception */
            for (auto p = exc; p.isNotNull(); p = p->parent())
            {
                if (p->parent().isNull())
                {
                    p->parent() = std::move(block->exception);
                    break;
                }
            }

            /* the first time exception occures */
            if (!(block->flags & EF_HANDLING))
            {
                /* set handing flags and exception object */
                block->flags |= EF_HANDLING;
                block->exception = std::move(exc);

                /* unwind the stack to the position before entering
                 * this recovery block, and jump to exception handling block */
                _stack.resize(block->sp);
                frame->jumpTo(block->except);
                break;
            }

            /* exceptions occured in "except" block */
            else if (!(block->flags & EF_FINALLY))
            {
                /* set flags for later rethrowing */
                block->flags &= ~EF_CAUGHT;
                block->exception = std::move(exc);

                /* unwind the stack to the position before entering
                 * this recovery block, and jump to "finally" block to cleanup */
                _stack.resize(block->sp);
                frame->jumpTo(block->finally);
                break;
            }

            /* exceptions occured in "finally" block */
            else
            {
                /* no more rescure blocks, we cannot handle it, propagate to parent;
                 * must write "throw exc" here, because a single "throw" will destroy the
                 * exception chaining information */
                if (blocks.size() == 1)
                    throw exc;

                /* otherwise, process the next block */
                blocks.pop();
                block = &(blocks.top());
            }
        }
    }
}
}
