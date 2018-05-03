#include <vector>

#include "utils/Strings.h"
#include "engine/Interpreter.h"

#include "runtime/MapObject.h"
#include "runtime/BoolObject.h"
#include "runtime/NullObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"
#include "runtime/NativeClassObject.h"

#include "runtime/ValueError.h"
#include "runtime/RuntimeError.h"
#include "runtime/InternalError.h"
#include "runtime/StopIteration.h"

#define OPERAND(p) ({ uint32_t v = *(uint32_t *)p; p += sizeof(uint32_t); v; })

namespace RedScript::Engine
{
Runtime::ObjectRef Interpreter::eval(Runtime::Reference<Runtime::CodeObject> code)
{
    /* bytecode pointers */
    const char *s = code->buffer().data();
    const char *p = code->buffer().data();
    const char *e = code->buffer().data() + code->buffer().size();

    /* runtime data structures */
    std::vector<Runtime::ObjectRef> stack;
    std::vector<Runtime::ObjectRef> locals(code->locals().size());

    /* loop until code ends */
    while (p < e)
    {
        auto line = code->lineNums()[p - s];
        OpCode opcode = static_cast<OpCode>(*p++);

        try
        {
            switch (opcode)
            {
                /* load null */
                case OpCode::LOAD_NULL:
                {
                    stack.emplace_back(Runtime::NullObject);
                    break;
                }

                /* load true */
                case OpCode::LOAD_TRUE:
                {
                    stack.emplace_back(Runtime::TrueObject);
                    break;
                }

                /* load false */
                case OpCode::LOAD_FALSE:
                {
                    stack.emplace_back(Runtime::FalseObject);
                    break;
                }

                /* load constant */
                case OpCode::LOAD_CONST:
                {
                    /* read constant ID */
                    uint32_t id = OPERAND(p);

                    /* check constant ID */
                    if (id >= code->consts().size())
                        throw Runtime::InternalError(Utils::Strings::format("Constant ID %u out of range", id));

                    /* load the constant into stack */
                    stack.emplace_back(code->consts()[id]);
                    break;
                }

                /* load by name */
                case OpCode::LOAD_NAME:
                {
                    // TODO: load by name
                    throw Runtime::InternalError("not implemented yet");
                }

                /* load local variable */
                case OpCode::LOAD_LOCAL:
                {
                    /* read local ID */
                    uint32_t id = OPERAND(p);

                    /* check local ID */
                    if (id >= locals.size())
                        throw Runtime::InternalError(Utils::Strings::format("Local ID %u out of range", id));

                    /* should have initialized */
                    if (locals[id].isNull())
                        throw Runtime::RuntimeError(Utils::Strings::format("Variable \"%s\" referenced before assignment", code->locals()[id]));

                    /* load the local into stack */
                    stack.emplace_back(locals[id]);
                    break;
                }

                /* set local variable */
                case OpCode::STOR_LOCAL:
                {
                    /* read local ID */
                    uint32_t id = OPERAND(p);

                    /* check stack */
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* check local ID */
                    if (id >= locals.size())
                        throw Runtime::InternalError(Utils::Strings::format("Local ID %u out of range", id));

                    /* set the local from stack */
                    locals[id] = std::move(stack.back());
                    stack.pop_back();
                    break;
                }

                /* delete local variable by setting to NULL */
                case OpCode::DEL_LOCAL:
                {
                    /* read local ID */
                    uint32_t id = OPERAND(p);

                    /* check local ID */
                    if (id >= locals.size())
                        throw Runtime::InternalError(Utils::Strings::format("Local ID %u out of range", id));

                    /* should have initialized */
                    if (locals[id].isNull())
                        throw Runtime::RuntimeError(Utils::Strings::format("Variable \"%s\" referenced before assignment", code->locals()[id]));

                    /* clear local reference */
                    locals[id] = nullptr;
                    break;
                }

                /* define attributes */
                case OpCode::DEF_ATTR:
                {
                    /* read local ID */
                    uint32_t id = OPERAND(p);

                    /* check stack */
                    if (stack.size() < 2)
                        throw Runtime::InternalError("Stack underflow");

                    /* check local ID */
                    if (id >= code->names().size())
                        throw Runtime::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                    /* pop top 2 elements */
                    Runtime::ObjectRef a = std::move(*(stack.end() - 2));
                    Runtime::ObjectRef b = std::move(*(stack.end() - 1));

                    /* define attribute from stack */
                    stack.resize(stack.size() - 2);
                    b->type()->objectDefineAttr(b, code->names()[id], a);
                    break;
                }

                /* get attributes */
                case OpCode::GET_ATTR:
                {
                    /* read local ID */
                    uint32_t id = OPERAND(p);

                    /* check stack */
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* check local ID */
                    if (id >= code->names().size())
                        throw Runtime::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                    /* replace stack top with new object */
                    stack.back() = stack.back()->type()->objectGetAttr(stack.back(), code->names()[id]);
                    break;
                }

                /* set attributes, error if not exists */
                case OpCode::SET_ATTR:
                {
                    /* read local ID */
                    uint32_t id = OPERAND(p);

                    /* check stack */
                    if (stack.size() < 2)
                        throw Runtime::InternalError("Stack underflow");

                    /* check local ID */
                    if (id >= code->names().size())
                        throw Runtime::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                    /* pop top 2 elements */
                    Runtime::ObjectRef a = std::move(*(stack.end() - 2));
                    Runtime::ObjectRef b = std::move(*(stack.end() - 1));

                    /* define attribute from stack */
                    stack.resize(stack.size() - 2);
                    b->type()->objectSetAttr(b, code->names()[id], a);
                    break;
                }

                /* remove attributes */
                case OpCode::DEL_ATTR:
                {
                    /* read local ID */
                    uint32_t id = OPERAND(p);

                    /* check stack */
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* check local ID */
                    if (id >= code->names().size())
                        throw Runtime::InternalError(Utils::Strings::format("Name ID %u out of range", id));

                    /* remove the attribute */
                    stack.back()->type()->objectDelAttr(stack.back(), code->names()[id]);
                    stack.pop_back();
                    break;
                }

                /* get by index */
                case OpCode::GET_ITEM:
                {
                    /* check stack */
                    if (stack.size() < 2)
                        throw Runtime::InternalError("Stack underflow");

                    /* pop top 2 elements */
                    Runtime::ObjectRef a = std::move(*(stack.end() - 2));
                    Runtime::ObjectRef b = std::move(*(stack.end() - 1));

                    /* replace stack top with new object */
                    stack.pop_back();
                    stack.back() = a->type()->sequenceGetItem(a, b);
                    break;
                }

                /* set by index */
                case OpCode::SET_ITEM:
                {
                    /* check stack */
                    if (stack.size() < 3)
                        throw Runtime::InternalError("Stack underflow");

                    /* pop top 3 elements */
                    Runtime::ObjectRef a = std::move(*(stack.end() - 3));
                    Runtime::ObjectRef b = std::move(*(stack.end() - 2));
                    Runtime::ObjectRef c = std::move(*(stack.end() - 1));

                    /* set the index from stack */
                    stack.resize(stack.size() - 3);
                    b->type()->sequenceSetItem(b, c, a);
                    break;
                }

                /* remove by index */
                case OpCode::DEL_ITEM:
                {
                    /* check stack */
                    if (stack.size() < 2)
                        throw Runtime::InternalError("Stack underflow");

                    /* pop top 2 elements */
                    Runtime::ObjectRef a = std::move(*(stack.end() - 2));
                    Runtime::ObjectRef b = std::move(*(stack.end() - 1));

                    /* delete index from stack */
                    stack.resize(stack.size() - 2);
                    a->type()->sequenceDelItem(a, b);
                    break;
                }

                /* return stack top */
                case OpCode::POP_RETURN:
                {
                    /* check for stack */
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* return the stack top */
                    // TODO: check exception handling blocks
                    Runtime::ObjectRef ret = std::move(stack.back());
                    stack.pop_back();
                    return std::move(ret);
                }

                /* invoke as function */
                case OpCode::CALL_FUNCTION:
                {
                    // TODO: implement this
                    throw Runtime::InternalError("not implemented yet");
                }

                /* duplicate stack top */
                case OpCode::DUP:
                {
                    /* check for stack */
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* pushing the top again */
                    stack.emplace_back(stack.back());
                    break;
                }

                /* duplicate top 2 elements of stack */
                case OpCode::DUP2:
                {
                    /* check for stack */
                    if (stack.size() < 2)
                        throw Runtime::InternalError("Stack underflow");

                    /* pushing the top two elements */
                    stack.emplace_back(*(stack.end() - 2));
                    stack.emplace_back(*(stack.end() - 2));
                    break;
                }

                /* discard stack top */
                case OpCode::DROP:
                {
                    /* check for stack */
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* discard stack top */
                    stack.pop_back();
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
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* retrive stack top */
                    Runtime::ObjectRef r;
                    Runtime::ObjectRef a = std::move(stack.back());

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"

                    /* dispatch opcode */
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
                    stack.back() = std::move(r);
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
                case OpCode::IN:
                {
                    /* check stack */
                    if (stack.size() < 2)
                        throw Runtime::InternalError("Stack underflow");

                    /* pop top 2 elements */
                    Runtime::ObjectRef r;
                    Runtime::ObjectRef a = std::move(*(stack.end() - 2));
                    Runtime::ObjectRef b = std::move(*(stack.end() - 1));
                    stack.pop_back();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"

                    /* dispatch operators */
                    switch (opcode)
                    {
                        case OpCode::ADD         : r = a->type()->numericAdd(a, b); break;
                        case OpCode::SUB         : r = a->type()->numericSub(a, b); break;
                        case OpCode::MUL         : r = a->type()->numericMul(a, b); break;
                        case OpCode::DIV         : r = a->type()->numericDiv(a, b); break;
                        case OpCode::MOD         : r = a->type()->numericMod(a, b); break;
                        case OpCode::POWER       : r = a->type()->numericPower(a, b); break;

                        case OpCode::BIT_OR      : r = a->type()->numericOr(a, b); break;
                        case OpCode::BIT_AND     : r = a->type()->numericAnd(a, b); break;
                        case OpCode::BIT_XOR     : r = a->type()->numericXor(a, b); break;

                        case OpCode::LSHIFT      : r = a->type()->numericLShift(a, b); break;
                        case OpCode::RSHIFT      : r = a->type()->numericRShift(a, b); break;

                        case OpCode::INP_ADD     : r = a->type()->numericIncAdd(a, b); break;
                        case OpCode::INP_SUB     : r = a->type()->numericIncSub(a, b); break;
                        case OpCode::INP_MUL     : r = a->type()->numericIncMul(a, b); break;
                        case OpCode::INP_DIV     : r = a->type()->numericIncDiv(a, b); break;
                        case OpCode::INP_MOD     : r = a->type()->numericIncMod(a, b); break;
                        case OpCode::INP_POWER   : r = a->type()->numericIncPower(a, b); break;

                        case OpCode::INP_BIT_OR  : r = a->type()->numericIncOr(a, b); break;
                        case OpCode::INP_BIT_AND : r = a->type()->numericIncAnd(a, b); break;
                        case OpCode::INP_BIT_XOR : r = a->type()->numericIncXor(a, b); break;

                        case OpCode::INP_LSHIFT  : r = a->type()->numericIncLShift(a, b); break;
                        case OpCode::INP_RSHIFT  : r = a->type()->numericIncRShift(a, b); break;

                        case OpCode::BOOL_OR     : r = a->type()->boolOr(a, b); break;
                        case OpCode::BOOL_AND    : r = a->type()->boolAnd(a, b); break;

                        case OpCode::EQ          : r = a->type()->comparableEq(a, b); break;
                        case OpCode::LT          : r = a->type()->comparableLt(a, b); break;
                        case OpCode::GT          : r = a->type()->comparableGt(a, b); break;
                        case OpCode::NEQ         : r = a->type()->comparableNeq(a, b); break;
                        case OpCode::LEQ         : r = a->type()->comparableLeq(a, b); break;
                        case OpCode::GEQ         : r = a->type()->comparableGeq(a, b); break;
                        case OpCode::IN          : r = a->type()->comparableContains(a, b); break;
                    }

#pragma clang diagnostic pop

                    /* replace the stack top */
                    stack.back() = std::move(r);
                    break;
                }

                /* exception matching */
                case OpCode::EXC_MATCH:
                {
                    // TODO: implement these
                    throw Runtime::InternalError("not implemented yet");
                }

                /* unconditional branch */
                case OpCode::BR:
                {
                    /* jump offset */
                    int32_t d = OPERAND(p);
                    const char *q = p + d - sizeof(uint32_t) - 1;

                    /* check the jump address */
                    if (q < s || q >= e)
                        throw Runtime::InternalError("Jump outside of code");

                    /* set the instruction pointer */
                    p = q;
                    break;
                }

                /* conditional branch */
                case OpCode::BRTRUE:
                case OpCode::BRFALSE:
                {
                    /* check stack */
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* jump offset */
                    int32_t d = OPERAND(p);
                    const char *q = p + d - sizeof(uint32_t) - 1;

                    /* check the jump address */
                    if (q < s || q >= e)
                        throw Runtime::InternalError("Jump outside of code");

                    /* set the instruction pointer if condition met */
                    if ((opcode == OpCode::BRTRUE) == stack.back()->type()->objectIsTrue(stack.back()))
                        p = q;

                    /* discard stack top */
                    stack.pop_back();
                    break;
                }

                /* throw exception */
                case OpCode::RAISE:
                {
                    // TODO: implement these
                    throw Runtime::InternalError("not implemented yet");
                }

                /* setup exception handling block */
                case OpCode::PUSH_BLOCK:
                {
                    // TODO: implement these
                    throw Runtime::InternalError("not implemented yet");
                }

                /* destroy exception handling block */
                case OpCode::POP_BLOCK:
                {
                    // TODO: implement these
                    throw Runtime::InternalError("not implemented yet");
                }

                /* advance iterator */
                case OpCode::ITER_NEXT:
                {
                    /* check stack */
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* push new object on stack top, don't replace iterator */
                    stack.emplace_back(stack.back()->type()->iterableNext(stack.back()));
                    break;
                }

                /* expand sequence in reverse order */
                case OpCode::EXPAND_SEQ:
                {
                    /* expected sequence size */
                    size_t i = 0;
                    uint32_t count = OPERAND(p);
                    std::vector<Runtime::ObjectRef> items(count);

                    /* check stack */
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* pop the stack top, convert to iterator */
                    auto iter = stack.back()->type()->iterableIter(stack.back());
                    stack.pop_back();

                    /* get all items from iterator */
                    try
                    {
                        while (i < count)
                        {
                            items[i] = iter->type()->iterableNext(iter);
                            i++;
                        }

                    /* iterator drained in the middle of expanding */
                    } catch (const Runtime::StopIteration &)
                    {
                        throw Runtime::ValueError(Utils::Strings::format(
                            "Needs %lu more items to unpack",
                            count - i
                        ));
                    }

                    /* and the iterator should have exact `count` items */
                    try
                    {
                        iter->type()->iterableNext(iter);
                        throw Runtime::ValueError("Too many items to unpack");

                    /* `StopIteration` is expected */
                    } catch (const Runtime::StopIteration &)
                    {
                        /* should have no more items */
                        /* this is expected, so ignore this exception */
                    }

                    break;
                }

                /* import module */
                case OpCode::IMPORT_ALIAS:
                {
                    // TODO: implement these
                    throw Runtime::InternalError("not implemented yet");
                }

                /* build a map object */
                case OpCode::MAKE_MAP:
                {
                    /* item count */
                    auto map = Runtime::Object::newObject<Runtime::MapObject>(Runtime::MapObject::Mode::Ordered);
                    uint32_t count = OPERAND(p);

                    /* check stack size */
                    if (stack.size() < count * 2)
                        throw Runtime::InternalError("Stack underflow");

                    /* build the map */
                    for (auto it = stack.end() - count * 2; it != stack.end(); it += 2)
                        map->insert(std::move(*it), std::move(*(it + 1)));

                    /* adjust the stack */
                    stack.resize(stack.size() - count);
                    stack.emplace_back(map);
                    break;
                }

                /* build an array object */
                case OpCode::MAKE_ARRAY:
                {
                    /* create array object */
                    auto size = OPERAND(p);
                    auto array = Runtime::Object::newObject<Runtime::ArrayObject>(size);

                    /* check stack */
                    if (stack.size() < size)
                        throw Runtime::InternalError("Stack underflow");

                    /* extract each item, in reverse order */
                    for (ssize_t i = size - 1; i >= 0; i--)
                        array->items()[i] = std::move(*(stack.end() - i - 1));

                    /* push result into stack */
                    stack.resize(stack.size() - size);
                    stack.emplace_back(std::move(array));
                    break;
                }

                /* build a tuple object */
                case OpCode::MAKE_TUPLE:
                {
                    /* create tuple object */
                    auto size = OPERAND(p);
                    auto tuple = Runtime::Object::newObject<Runtime::TupleObject>(size);

                    /* check stack */
                    if (stack.size() < size)
                        throw Runtime::InternalError("Stack underflow");

                    /* extract each item, in reverse order */
                    for (ssize_t i = size - 1; i >= 0; i--)
                        tuple->items()[i] = std::move(*(stack.end() - i - 1));

                    /* push result into stack */
                    stack.resize(stack.size() - size);
                    stack.emplace_back(std::move(tuple));
                    break;
                }

                /* build a function object */
                case OpCode::MAKE_FUNCTION:
                {
                    // TODO: implement these
                    throw Runtime::InternalError("not implemented yet");
                }

                /* build a class object */
                case OpCode::MAKE_CLASS:
                {
                    // TODO: implement these
                    throw Runtime::InternalError("not implemented yet");
                }

                /* build a native class object */
                case OpCode::MAKE_NATIVE:
                {
                    /* extract constant ID */
                    uint32_t id = OPERAND(p);

                    /* check stack */
                    if (stack.empty())
                        throw Runtime::InternalError("Stack is empty");

                    /* check operand range */
                    if (id >= code->consts().size())
                        throw Runtime::InternalError("Constant ID out of range");

                    /* load options from stack */
                    Runtime::ObjectRef source = code->consts()[id];
                    Runtime::ObjectRef options = std::move(stack.back());

                    /* must be a map */
                    if (source->type() != Runtime::StringTypeObject)
                        throw Runtime::InternalError("Invalid source string");

                    /* must be a map */
                    if (options->type() != Runtime::MapTypeObject)
                        throw Runtime::InternalError("Invalid options map");

                    /* replace stack top with native class object */
                    stack.back() = Runtime::Object::newObject<Runtime::NativeClassObject>(
                        source.as<Runtime::StringObject>()->value(),
                        options.as<Runtime::MapObject>()
                    );

                    break;
                }
            }
        } catch (const std::exception &e)
        {
            // TODO: exception recovery
            throw;
        }
    }

    /* really should not get here */
    throw Runtime::InternalError("Unexpected termination of eval loop");
}
}
