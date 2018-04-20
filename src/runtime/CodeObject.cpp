#include "runtime/CodeObject.h"
#include "runtime/RuntimeError.h"

namespace RedScript::Runtime
{
/* type object for code */
TypeRef CodeTypeObject;

uint32_t CodeObject::emit(Engine::OpCode op)
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

uint32_t CodeObject::addConst(Runtime::ObjectRef value)
{
    /* current constant ID */
    auto it = _consts.find(value);
    size_t p = _constTable.size();

    /* already exists, return the existing ID */
    if (it != _consts.end())
        return it->second;

    /* each code object can have at most 4G constants */
    if (p >= UINT32_MAX)
        throw Runtime::RuntimeError("Too many constants");

    /* add to constant table */
    _constTable.emplace_back(value);
    _consts.emplace(value, static_cast<uint32_t>(p));
    return static_cast<uint32_t>(p);
}

uint32_t CodeObject::addLocal(const std::string &value)
{
    /* if not exists, add to locals */
    if (_locals.find(value) == _locals.end())
        _locals.emplace(value, addString(value));

    /* return the variable ID */
    return _locals[value];
}

uint32_t CodeObject::addString(const std::string &value)
{
    /* current constant ID */
    auto it = _names.find(value);
    size_t p = _nameTable.size();

    /* already exists, return the existing ID */
    if (it != _names.end())
        return it->second;

    /* each code object can have at most 4G strings */
    if (p >= UINT32_MAX)
        throw Runtime::RuntimeError("Too many strings");

    /* add to string table */
    _nameTable.emplace_back(value);
    _names.emplace(value, static_cast<uint32_t>(p));
    return static_cast<uint32_t>(p);
}

uint32_t CodeObject::emitJump(Engine::OpCode op)
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

uint32_t CodeObject::emitOperand(Engine::OpCode op, int32_t operand)
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

void CodeObject::initialize(void)
{
    /* code type object */
    static CodeType _codeType;
    CodeTypeObject = Reference<CodeType>::refStatic(_codeType);
}
}
