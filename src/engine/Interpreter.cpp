#include "utils/Strings.h"
#include "engine/Interpreter.h"

#include "runtime/MapObject.h"
#include "runtime/BoolObject.h"
#include "runtime/NullObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"
#include "runtime/FunctionObject.h"
#include "runtime/NativeClassObject.h"

#include "exceptions/NameError.h"
#include "exceptions/TypeError.h"
#include "exceptions/ValueError.h"
#include "exceptions/RuntimeError.h"
#include "exceptions/StopIteration.h"

#define OPERAND() ({                                                            \
    auto v = p;                                                                 \
    p += sizeof(uint32_t);                                                      \
    if (p >= e) throw Exceptions::InternalError("Unexpected end of bytecode");  \
    *reinterpret_cast<const uint32_t *>(v);                                     \
})

namespace
{
template <typename T>
struct CounterScope
{
    T &val;
   ~CounterScope() { val--; }
    CounterScope(T &val) : val(val) { val++; }
};
}

namespace RedScript::Engine
{
Runtime::ObjectRef Interpreter::tupleConcat(Runtime::ObjectRef a, Runtime::ObjectRef b)
{
    /* both are null */
    if (a.isNull() && b.isNull())
        return Runtime::TupleObject::newEmpty();

    /* a == null, b != null */
    if (a.isNull() && !(b.isNull()))
        return b;

    /* b == null, a != null */
    if (b.isNull() && !(a.isNull()))
        return a;

    /* check for left tuple, right tuple already checked before */
    if (a->isNotInstanceOf(Runtime::TupleTypeObject))
        throw Exceptions::InternalError("Invalid left tuple object");

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
    if (a.isNull() && !(b.isNull()))
        return b;

    /* b == null, a != null */
    if (b.isNull() && !(a.isNull()))
        return a;

    /* check for left map, right map already checked before */
    if (a->isNotInstanceOf(Runtime::MapTypeObject))
        throw Exceptions::InternalError("Invalid left map object");

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

Runtime::ObjectRef Interpreter::eval(void)
{
    /* recursion counter */
    static thread_local ssize_t depth = 0;
    CounterScope<ssize_t> recursion(depth);

    /* check for recursion depth */
    if (recursion.val >= 2048)
        throw Exceptions::RuntimeError("Maximum recursion depth exceeded");

    /* bytecode pointers */
    const char *s = _code->buffer().data();
    const char *p = _code->buffer().data();
    const char *e = _code->buffer().data() + _code->buffer().size();

    /* loop until code ends */
    while (p < e)
    {
        auto line = _code->lineNums()[p - s];
        OpCode opcode = static_cast<OpCode>(*p++);

        try
        {
            switch (opcode)
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
                    uint32_t id = OPERAND();

                    /* check constant ID */
                    if (id >= _code->consts().size())
                        throw Exceptions::InternalError(Utils::Strings::format("Constant ID %u out of range", id));

                    /* load the constant into stack */
                    _stack.emplace_back(_code->consts()[id]);
                    break;
                }

                /* load by name */
                case OpCode::LOAD_NAME:
                {
                    /* read name ID */
                    uint32_t id = OPERAND();

                    /* check name ID */
                    if (id >= _code->names().size())
                        throw Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                    /* find name in locals */
                    auto name = _code->names()[id];
                    auto iter = _code->localMap().find(name);

                    /* found, use the local value */
                    if ((iter != _code->localMap().end()))
                    {
                        /* must be assigned before using */
                        if (_locals[iter->second].isNull())
                        {
                            throw Exceptions::NameError(
                                line.first,
                                line.second,
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
                            throw Exceptions::NameError(
                                line.first,
                                line.second,
                                Utils::Strings::format("Variable \"%s\" referenced before assignment", name)
                            );
                        }

                        /* push onto the stack */
                        _stack.emplace_back(cliter->second->get());
                        break;
                    }

                    /* otherwise, it's an error */
                    throw Exceptions::NameError(
                        line.first,
                        line.second,
                        Utils::Strings::format("\"%s\" is not defined", name)
                    );
                }

                /* load local variable */
                case OpCode::LOAD_LOCAL:
                {
                    /* read local ID */
                    uint32_t id = OPERAND();

                    /* check local ID */
                    if (id >= _locals.size())
                        throw Exceptions::InternalError(Utils::Strings::format("Local ID %u out of range", id));

                    /* should have initialized */
                    if (_locals[id].isNull())
                    {
                        throw Exceptions::NameError(
                            line.first,
                            line.second,
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
                    uint32_t id = OPERAND();

                    /* check stack */
                    if (_stack.empty())
                        throw Exceptions::InternalError("Stack is empty");

                    /* check local ID */
                    if (id >= _locals.size())
                        throw Exceptions::InternalError(Utils::Strings::format("Local ID %u out of range", id));

                    /* set the local from stack */
                    _locals[id] = std::move(_stack.back());
                    _stack.pop_back();
                    break;
                }

                /* delete local variable by setting to NULL */
                case OpCode::DEL_LOCAL:
                {
                    /* read local ID */
                    uint32_t id = OPERAND();

                    /* check local ID */
                    if (id >= _locals.size())
                        throw Exceptions::InternalError(Utils::Strings::format("Local ID %u out of range", id));

                    /* should have initialized */
                    if (_locals[id].isNull())
                    {
                        throw Exceptions::NameError(
                            line.first,
                            line.second,
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
                    uint32_t id = OPERAND();

                    /* check stack */
                    if (_stack.size() < 2)
                        throw Exceptions::InternalError("Stack underflow");

                    /* check local ID */
                    if (id >= _code->names().size())
                        throw Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", id));

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
                    uint32_t id = OPERAND();

                    /* check stack */
                    if (_stack.empty())
                        throw Exceptions::InternalError("Stack is empty");

                    /* check local ID */
                    if (id >= _code->names().size())
                        throw Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                    /* replace stack top with new object */
                    _stack.back() = _stack.back()->type()->objectGetAttr(_stack.back(), _code->names()[id]);
                    break;
                }

                /* set attributes, error if not exists */
                case OpCode::SET_ATTR:
                {
                    /* read local ID */
                    uint32_t id = OPERAND();

                    /* check stack */
                    if (_stack.size() < 2)
                        throw Exceptions::InternalError("Stack underflow");

                    /* check local ID */
                    if (id >= _code->names().size())
                        throw Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", id));

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
                    uint32_t id = OPERAND();

                    /* check stack */
                    if (_stack.empty())
                        throw Exceptions::InternalError("Stack is empty");

                    /* check local ID */
                    if (id >= _code->names().size())
                        throw Exceptions::InternalError(Utils::Strings::format("Name ID %u out of range", id));

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
                        throw Exceptions::InternalError("Stack underflow");

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
                        throw Exceptions::InternalError("Stack underflow");

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
                        throw Exceptions::InternalError("Stack underflow");

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
                    uint32_t flags = OPERAND();
                    Runtime::ObjectRef end = nullptr;
                    Runtime::ObjectRef step = nullptr;
                    Runtime::ObjectRef begin = nullptr;

                    /* read stepping if any */
                    if (flags & Engine::SL_STEP)
                    {
                        /* check stack */
                        if (_stack.empty())
                            throw Exceptions::InternalError("Stack is empty");

                        /* pop from stack */
                        step = std::move(_stack.back());
                        _stack.pop_back();
                    }

                    /* read ending if any */
                    if (flags & Engine::SL_END)
                    {
                        /* check stack */
                        if (_stack.empty())
                            throw Exceptions::InternalError("Stack is empty");

                        /* pop from stack */
                        end = std::move(_stack.back());
                        _stack.pop_back();
                    }

                    /* read beginning if any */
                    if (flags & Engine::SL_BEGIN)
                    {
                        /* check stack */
                        if (_stack.empty())
                            throw Exceptions::InternalError("Stack is empty");

                        /* pop from stack */
                        begin = std::move(_stack.back());
                        _stack.pop_back();
                    }

                    /* check stack */
                    if (_stack.empty())
                        throw Exceptions::InternalError("Stack is empty");

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
                        throw Exceptions::InternalError("Stack is empty");

                    // TODO: check exception handling blocks

                    /* pop the stack top, that's the return value */
                    Runtime::ObjectRef ret = std::move(_stack.back());
                    _stack.pop_back();

                    /* should have no items left */
                    if (!(_stack.empty()))
                        throw Exceptions::InternalError("Stack not empty when return");

                    /* move to prevent copy */
                    return std::move(ret);
                }

                /* invoke as function */
                case OpCode::CALL_FUNCTION:
                {
                    /* invocation flags */
                    uint32_t flags = OPERAND();
                    Runtime::ObjectRef obj = nullptr;
                    Runtime::ObjectRef vargs = nullptr;
                    Runtime::ObjectRef kwargs = nullptr;

                    /* decorator invocation */
                    if (flags & FI_DECORATOR)
                    {
                        /* should be the only flag */
                        if (flags != FI_DECORATOR)
                            throw Exceptions::InternalError(Utils::Strings::format("Invalid invocation flags 0x%.8x", flags));

                        /* check for stack */
                        if (_stack.empty())
                            throw Exceptions::InternalError("Stack is empty");

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
                                throw Exceptions::InternalError("Stack is empty");

                            /* pop keyword arguments from stack */
                            kwargs = std::move(_stack.back());
                            _stack.pop_back();

                            /* must be a map */
                            if (kwargs->isNotInstanceOf(Runtime::MapTypeObject))
                                throw Exceptions::TypeError("Keyword argument pack must be a map");
                        }

                        /* have variadic arguments */
                        if (flags & FI_VARGS)
                        {
                            /* check for stack */
                            if (_stack.empty())
                                throw Exceptions::InternalError("Stack is empty");

                            /* pop variadic arguments from stack */
                            vargs = std::move(_stack.back());
                            _stack.pop_back();

                            /* must be a tuple */
                            if (vargs->isNotInstanceOf(Runtime::TupleTypeObject))
                                throw Exceptions::TypeError("Variadic argument pack must be a tuple");
                        }

                        /* have named arguments */
                        if (flags & FI_NAMED)
                        {
                            /* check for stack */
                            if (_stack.empty())
                                throw Exceptions::InternalError("Stack is empty");

                            /* pop named arguments from stack */
                            named = std::move(_stack.back());
                            _stack.pop_back();
                        }

                        /* have arguments */
                        if (flags & FI_ARGS)
                        {
                            /* check for stack */
                            if (_stack.empty())
                                throw Exceptions::InternalError("Stack is empty");

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
                        throw Exceptions::InternalError("Stack is empty");

                    /* pop callable object from stack, and invoke it */
                    obj = std::move(_stack.back());
                    _stack.back() = obj->type()->objectInvoke(obj, vargs, kwargs);
                    break;
                }

                /* duplicate stack top */
                case OpCode::DUP:
                {
                    /* check for stack */
                    if (_stack.empty())
                        throw Exceptions::InternalError("Stack is empty");

                    /* pushing the top again */
                    _stack.emplace_back(_stack.back());
                    break;
                }

                /* duplicate top 2 elements of stack */
                case OpCode::DUP2:
                {
                    /* check for stack */
                    if (_stack.size() < 2)
                        throw Exceptions::InternalError("Stack underflow");

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
                        throw Exceptions::InternalError("Stack is empty");

                    /* discard stack top */
                    _stack.pop_back();
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
                        throw Exceptions::InternalError("Stack is empty");

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
                        throw Exceptions::InternalError("Stack underflow");

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
                            r = Runtime::BoolObject::fromBool(a.get() == b.get());
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

                /* exception matching */
                case OpCode::EXC_MATCH:
                {
                    // TODO: implement these
                    throw Exceptions::InternalError("not implemented yet");
                }

                /* unconditional branch */
                case OpCode::BR:
                {
                    /* jump offset */
                    int32_t d = OPERAND();
                    const char *q = p + d - sizeof(uint32_t) - 1;

                    /* check the jump address */
                    if (q < s || q >= e)
                        throw Exceptions::InternalError("Jump outside of code");

                    /* set the instruction pointer */
                    p = q;
                    break;
                }

                /* conditional branch */
                case OpCode::BRTRUE:
                case OpCode::BRFALSE:
                {
                    /* check stack */
                    if (_stack.empty())
                        throw Exceptions::InternalError("Stack is empty");

                    /* jump offset */
                    int32_t d = OPERAND();
                    const char *q = p + d - sizeof(uint32_t) - 1;

                    /* check the jump address */
                    if (q < s || q >= e)
                        throw Exceptions::InternalError("Jump outside of code");

                    /* set the instruction pointer if condition met */
                    if ((opcode == OpCode::BRTRUE) == _stack.back()->type()->objectIsTrue(_stack.back()))
                        p = q;

                    /* discard stack top */
                    _stack.pop_back();
                    break;
                }

                /* throw exception */
                case OpCode::RAISE:
                {
                    // TODO: implement these
                    throw Exceptions::InternalError("not implemented yet");
                }

                /* setup exception handling block */
                case OpCode::PUSH_BLOCK:
                {
                    // TODO: implement these
                    throw Exceptions::InternalError("not implemented yet");
                }

                /* destroy exception handling block */
                case OpCode::POP_BLOCK:
                {
                    // TODO: implement these
                    throw Exceptions::InternalError("not implemented yet");
                }

                /* end of the finally block */
                case OpCode::END_FINALLY:
                {
                    // TODO: implement these
                    throw Exceptions::InternalError("not implemented yet");
                }

                /* advance iterator */
                case OpCode::ITER_NEXT:
                {
                    /* check stack */
                    if (_stack.empty())
                        throw Exceptions::InternalError("Stack is empty");

                    /* jump address when iterator drained */
                    int32_t d = OPERAND();
                    const char *q = p + d - sizeof(uint32_t) - 1;

                    /* try push new object on stack top, don't destroy the iterator */
                    try
                    {
                        _stack.emplace_back(_stack.back()->type()->iterableNext(_stack.back()));
                        break;
                    }

                    /* iterator drained, drop the iterator, and jump to address */
                    catch (const Exceptions::StopIteration &)
                    {
                        p = q;
                        _stack.pop_back();
                        break;
                    }
                }

                /* expand sequence in reverse order */
                case OpCode::EXPAND_SEQ:
                {
                    /* expected sequence size */
                    uint32_t count = OPERAND();

                    /* check stack */
                    if (_stack.empty())
                        throw Exceptions::InternalError("Stack is empty");

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
                            throw Exceptions::ValueError(Utils::Strings::format(
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
                        catch (const Exceptions::StopIteration &)
                        {
                            throw Exceptions::ValueError(Utils::Strings::format(
                                "Needs %zu more items to unpack",
                                count - i
                            ));
                        }

                        /* and the iterator should have exact `count` items */
                        try
                        {
                            iter->type()->iterableNext(iter);
                            throw Exceptions::ValueError("Too many items to unpack");
                        }

                        /* `StopIteration` is expected */
                        catch (const Exceptions::StopIteration &)
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
                    throw Exceptions::InternalError("not implemented yet");
                }

                /* build a map object */
                case OpCode::MAKE_MAP:
                {
                    /* item count */
                    auto map = Runtime::MapObject::newOrdered();
                    uint32_t count = OPERAND();

                    /* check stack size */
                    if (_stack.size() < count * 2)
                        throw Exceptions::InternalError("Stack underflow");

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
                    auto size = OPERAND();
                    auto iter = _stack.rbegin();
                    auto array = Runtime::Object::newObject<Runtime::ArrayObject>(size);

                    /* check stack */
                    if (_stack.size() < size)
                        throw Exceptions::InternalError("Stack underflow");

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
                    auto size = OPERAND();
                    auto iter = _stack.rbegin();
                    auto tuple = Runtime::TupleObject::fromSize(size);

                    /* check stack */
                    if (_stack.size() < size)
                        throw Exceptions::InternalError("Stack underflow");

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
                        throw Exceptions::InternalError("Stack is empty");

                    /* code object ID and defaults pack */
                    auto id = OPERAND();
                    auto def = std::move(_stack.back());

                    /* check constant ID */
                    if (id >= _code->consts().size())
                        throw Exceptions::InternalError(Utils::Strings::format("Constant ID %u out of range", id));

                    /* check for tuple object */
                    if (def->isNotInstanceOf(Runtime::TupleTypeObject))
                        throw Exceptions::InternalError("Invalid tuple object");

                    /* check for code object */
                    if (_code->consts()[id]->isNotInstanceOf(Runtime::CodeTypeObject))
                        throw Exceptions::InternalError("Invalid code object");

                    /* convert to corresponding type */
                    auto funcDef = def.as<Runtime::TupleObject>();
                    auto funcCode = _code->consts()[id].as<Runtime::CodeObject>();
                    std::unordered_map<std::string, Engine::ClosureRef> funcClosure;

                    /* build function closure */
                    for (const auto &item : funcCode->names())
                    {
                        /* find from local variables first */
                        auto iter = _code->localMap().find(item);

                        /* it's a local variable, wrap it as closure */
                        if (iter != _code->localMap().end())
                        {
                            _closures[iter->second] = std::make_unique<Closure::Context>(&_locals, iter->second);
                            funcClosure.emplace(item, _closures[iter->second]->ref);
                        }
                        else
                        {
                            /* otherwise, find in closure map */
                            auto cliter = _names.find(item);

                            /* if found, inherit the closure */
                            if (cliter != _names.end())
                                funcClosure.emplace(item, cliter->second);

                            /* if not found, it should never be found again,
                             * but we don't throw exceptions here, the opcode
                             * LOAD_NAME will throw an exception for us when it
                             * comes to resolving the name */
                        }
                    }

                    /* create function object, and put on stack */
                    _stack.back() = Runtime::Object::newObject<Runtime::FunctionObject>(funcCode, funcDef, funcClosure);
                    break;
                }

                /* build a class object */
                case OpCode::MAKE_CLASS:
                {
                    // TODO: implement these
                    throw Exceptions::InternalError("not implemented yet");
                }

                /* build a native class object */
                case OpCode::MAKE_NATIVE:
                {
                    /* extract constant ID */
                    uint32_t id = OPERAND();

                    /* check stack */
                    if (_stack.empty())
                        throw Exceptions::InternalError("Stack is empty");

                    /* check operand range */
                    if (id >= _code->consts().size())
                        throw Exceptions::InternalError("Constant ID out of range");

                    /* load options from stack */
                    Runtime::ObjectRef source = _code->consts()[id];
                    Runtime::ObjectRef options = std::move(_stack.back());

                    /* must be a map */
                    if (source->type() != Runtime::StringTypeObject)
                        throw Exceptions::InternalError("Invalid source string");

                    /* must be a map */
                    if (options->type() != Runtime::MapTypeObject)
                        throw Exceptions::InternalError("Invalid options map");

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
                    throw Exceptions::InternalError(Utils::Strings::format(
                        "Invalid op-code %.2x",
                        static_cast<uint8_t>(opcode)
                    ));
                }

#pragma clang diagnostic pop

            }
        }

        /* exceptions occured when executing, try exception recovery */
        catch (const std::logic_error &e)
        {
            // TODO: exception recovery
            throw;
        }
    }

    /* really should not get here */
    throw Exceptions::InternalError("Unexpected termination of eval loop");
}
}

#undef OPERAND
