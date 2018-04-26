#include <vector>

#include "utils/Strings.h"
#include "engine/Interpreter.h"

#include "runtime/BoolObject.h"
#include "runtime/NullObject.h"

#include "runtime/RuntimeError.h"
#include "runtime/InternalError.h"

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

                    /* load the local into stack */
                    locals[id] = nullptr;
                    break;
                }

                /* define attributes */
                case OpCode::DEF_ATTR:

                /* get attributes */
                case OpCode::GET_ATTR:

                /* set attributes, error if not exists */
                case OpCode::SET_ATTR:

                /* remove attributes */
                case OpCode::DEL_ATTR:

                /* get by index */
                case OpCode::GET_ITEM:

                /* set by index */
                case OpCode::SET_ITEM:

                /* remove by index */
                case OpCode::DEL_ITEM:
                {
                    // TODO: implement these
                    throw Runtime::InternalError("not implemented yet");
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
                        throw Runtime::InternalError("Stack is empty");

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
                {
                    /* retrive stack top */
                    Runtime::ObjectRef r;
                    Runtime::ObjectRef a = std::move(stack.back());

                    /* dispatch opcode */
                    switch (opcode)
                    {
                        case OpCode::POS      : r = a->type()->numericPos(a); break;
                        case OpCode::NEG      : r = a->type()->numericNeg(a); break;
                        case OpCode::BIT_NOT  : r = a->type()->numericNot(a); break;
                        case OpCode::BOOL_NOT : r = a->type()->boolNot(a); break;

                        /* never happens */
                        default:
                            throw Runtime::InternalError("Invalid unary operator instruction");
                    }

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
                    /* pop top 2 elements */
                    Runtime::ObjectRef r;
                    Runtime::ObjectRef a = std::move(*(stack.end() - 2));
                    Runtime::ObjectRef b = std::move(*(stack.end() - 1));
                    stack.pop_back();

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

                        /* never happens */
                        default:
                            throw Runtime::InternalError("Invalid binary operator instruction");
                    }

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

                /* setup exception handling block */
                case OpCode::PUSH_BLOCK:

                /* destroy exception handling block */
                case OpCode::POP_BLOCK:
                {
                    // TODO: implement these
                    throw Runtime::InternalError("not implemented yet");
                }

                /* advance iterator */
                case OpCode::ITER_NEXT:
                {
                    stack.emplace_back(stack.back()->type()->iterableNext(stack.back()));
                    break;
                }

                /* expand sequence in reverse order */
                case OpCode::EXPAND_SEQ:

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
                    uint32_t n = OPERAND(p);
                    Runtime::ObjectRef map = Runtime::NullObject;

                    // TODO: build an actual map
                    if (n)
                        throw Runtime::InternalError("not implemented yet");

                    /* adjust the stack */
                    stack.resize(stack.size() - n);
                    stack.emplace_back(map);
                    break;
                }

                /* build an array object */
                case OpCode::MAKE_ARRAY:

                /* build a tuple object */
                case OpCode::MAKE_TUPLE:

                /* build a function object */
                case OpCode::MAKE_FUNCTION:

                /* build a class object */
                case OpCode::MAKE_CLASS:
                {
                    // TODO: implement these
                    throw Runtime::InternalError("not implemented yet");
                }

                /* convert stack top into iterator */
                case OpCode::MAKE_ITER:
                {
                    Runtime::ObjectRef a = std::move(stack.back());
                    stack.back() = std::move(a->type()->iterableIter(a));
                    break;
                }

                /* build a native class object */
                case OpCode::MAKE_NATIVE:
                {
                    stack.emplace_back(Runtime::NullObject);
                    break;
//                    throw Runtime::InternalError("native class not implemented yet");
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
