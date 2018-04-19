#include <stdexcept>

#include "runtime/IntObject.h"
#include "runtime/CodeObject.h"
#include "runtime/StringObject.h"
#include "runtime/DecimalObject.h"

#include "runtime/RuntimeError.h"
#include "runtime/InternalError.h"
#include "compiler/CodeGenerator.h"

namespace RedScript::Compiler
{
uint32_t CodeGenerator::emit(Engine::OpCode op)
{
    /* current instruction pointer */
    size_t p = _buffer.size();

    /* each code object is limited to 4G */
    if (p >= UINT32_MAX)
        throw Runtime::RuntimeError("Code exceeds 4G limit");

    /* add to instruction buffer */
    _buffer.emplace_back(static_cast<uint8_t>(op));
    return static_cast<uint32_t>(p);
}

uint32_t CodeGenerator::addConst(Runtime::ObjectRef value)
{
    /* current constant ID */
    size_t p = _consts.size();

    /* each code object can have at most 4G constants */
    if (p >= UINT32_MAX)
        throw Runtime::RuntimeError("Too many constants");

    /* add to constant table */
    _consts.emplace_back(value);
    return static_cast<uint32_t>(p);
}

uint32_t CodeGenerator::addString(const std::string &value)
{
    /* current constant ID */
    size_t p = _strings.size();

    /* each code object can have at most 4G strings */
    if (p >= UINT32_MAX)
        throw Runtime::RuntimeError("Too many strings");

    /* add to string table */
    _strings.emplace_back(value);
    return static_cast<uint32_t>(p);
}

uint32_t CodeGenerator::emitJump(Engine::OpCode op)
{
    /* emit the opcode */
    size_t p = emit(op);

    /* check for operand space */
    if (p >= UINT32_MAX - sizeof(int32_t) - 1)
        throw Runtime::RuntimeError("Code exceeds 4G limit");

    /* preserve space in instruction buffer */
    _buffer.resize(p + sizeof(int32_t) + 1);
    return static_cast<uint32_t>(p) + 1;
}

uint32_t CodeGenerator::emitOperand(Engine::OpCode op, int32_t operand)
{
    /* emit the opcode */
    char *v = reinterpret_cast<char *>(&operand);
    size_t p = emit(op);

    /* check for operand space */
    if (p >= UINT32_MAX - sizeof(int32_t) - 1)
        throw Runtime::RuntimeError("Code exceeds 4G limit");

    /* add operand to instruction buffer */
    _buffer.insert(_buffer.end(), v, v + sizeof(int32_t));
    return static_cast<uint32_t>(p);
}

Runtime::ObjectRef CodeGenerator::build(void)
{
    /* generate code if not generated */
    if (_code.isNull())
    {
        _code = Runtime::Object::newObject<Runtime::CodeObject>();
        visitCompoundStatement(_block);
    }

    /* return the code reference */
    return _code;
}

void CodeGenerator::visitLiteral(const std::unique_ptr<AST::Literal> &node)
{
    Runtime::ObjectRef val;

    /* construct corresponding type */
    switch (node->vtype)
    {
        case AST::Literal::Type::String: val = Runtime::Object::newObject<Runtime::StringObject>(node->string); break;
        case AST::Literal::Type::Decimal: val = Runtime::Object::newObject<Runtime::DecimalObject>(node->decimal); break;
        case AST::Literal::Type::Integer: val = Runtime::Object::newObject<Runtime::IntObject>(node->integer); break;
    }

    /* emit opcode with operand */
    emitOperand(Engine::OpCode::LOAD_CONST, addConst(val));
}

void CodeGenerator::visitExpression(const std::unique_ptr<AST::Expression> &node)
{
    std::vector<size_t> patches;

    /* first expression */
    switch (node->first.type)
    {
        case AST::Expression::Operand::Type::Composite  : visitComposite(node->first.composite); break;
        case AST::Expression::Operand::Type::Expression : visitExpression(node->first.expression); break;
    }

    /* remaining expression parts */
    for (const auto &item : node->follows)
    {
        /* short-circuited boolean operation */
        switch (item.op)
        {
            /* boolean or, with short-circuit evaluation */
            case Token::Operator::BoolOr:
            {
                patches.emplace_back(emitJump(Engine::OpCode::BRTRUE));
                break;
            }

            /* boolean and, with short-circuit evaluation */
            case Token::Operator::BoolAnd:
            {
                patches.emplace_back(emitJump(Engine::OpCode::BRFALSE));
                break;
            }

            /* other operators, don't care */
            default:
                break;
        }

        /* next term */
        switch (item.type)
        {
            case AST::Expression::Operand::Type::Composite  : visitComposite(item.composite); break;
            case AST::Expression::Operand::Type::Expression : visitExpression(item.expression); break;
        }

        /* emit operators */
        switch (item.op)
        {
            /* comparison operators */
            case Token::Operator::Less              : emit(Engine::OpCode::LE);          break;
            case Token::Operator::Greater           : emit(Engine::OpCode::GE);          break;
            case Token::Operator::Leq               : emit(Engine::OpCode::LEQ);         break;
            case Token::Operator::Geq               : emit(Engine::OpCode::LEQ);         break;
            case Token::Operator::Equ               : emit(Engine::OpCode::EQ);          break;
            case Token::Operator::Neq               : emit(Engine::OpCode::NEQ);         break;
            case Token::Operator::In                : emit(Engine::OpCode::IN);          break;

            /* boolean operators, with short-circuit evaluation */
            case Token::Operator::BoolOr            : emit(Engine::OpCode::BOOL_OR);     break;
            case Token::Operator::BoolAnd           : emit(Engine::OpCode::BOOL_AND);    break;

            /* basic arithmetic operators */
            case Token::Operator::Plus              : emit(Engine::OpCode::ADD);         break;
            case Token::Operator::Minus             : emit(Engine::OpCode::SUB);         break;
            case Token::Operator::Divide            : emit(Engine::OpCode::DIV);         break;
            case Token::Operator::Multiply          : emit(Engine::OpCode::MUL);         break;
            case Token::Operator::Module            : emit(Engine::OpCode::MOD);         break;
            case Token::Operator::Power             : emit(Engine::OpCode::POWER);       break;

            /* bit manipulation operators */
            case Token::Operator::BitAnd            : emit(Engine::OpCode::BIT_AND);     break;
            case Token::Operator::BitOr             : emit(Engine::OpCode::BIT_OR);      break;
            case Token::Operator::BitXor            : emit(Engine::OpCode::BIT_XOR);     break;

            /* bit shifting operators */
            case Token::Operator::ShiftLeft         : emit(Engine::OpCode::LSHIFT);      break;
            case Token::Operator::ShiftRight        : emit(Engine::OpCode::RSHIFT);      break;

            /* inplace basic arithmetic operators */
            case Token::Operator::InplaceAdd        : emit(Engine::OpCode::INP_ADD);     break;
            case Token::Operator::InplaceSub        : emit(Engine::OpCode::INP_SUB);     break;
            case Token::Operator::InplaceMul        : emit(Engine::OpCode::INP_MUL);     break;
            case Token::Operator::InplaceDiv        : emit(Engine::OpCode::INP_DIV);     break;
            case Token::Operator::InplaceMod        : emit(Engine::OpCode::INP_MOD);     break;
            case Token::Operator::InplacePower      : emit(Engine::OpCode::INP_POWER);   break;

            /* inplace bit manipulation operators */
            case Token::Operator::InplaceBitAnd     : emit(Engine::OpCode::INP_BIT_AND); break;
            case Token::Operator::InplaceBitOr      : emit(Engine::OpCode::INP_BIT_OR);  break;
            case Token::Operator::InplaceBitXor     : emit(Engine::OpCode::INP_BIT_XOR); break;

            /* inplace bit shifting operators */
            case Token::Operator::InplaceShiftLeft  : emit(Engine::OpCode::INP_LSHIFT);  break;
            case Token::Operator::InplaceShiftRight : emit(Engine::OpCode::INP_RSHIFT);  break;

            default:
                throw Runtime::InternalError(Utils::Strings::format("Impossible operator %s", Token::toString(item.op)));
        }
    }

    /* patch all jump instructions */
    for (const auto &p : patches)
        *(int32_t *)(&_buffer[p]) = static_cast<int32_t>(_buffer.size() - p + 1);
}
}
