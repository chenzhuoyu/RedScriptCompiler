#include "runtime/CodeObject.h"
#include "runtime/RuntimeError.h"

namespace RedScript::Runtime
{
/* type object for code */
TypeRef CodeTypeObject;

uint32_t CodeObject::addName(const std::string &name)
{
    /* current constant ID */
    auto it = _names.find(name);
    size_t p = _nameTable.size();

    /* already exists, return the existing ID */
    if (it != _names.end())
        return it->second;

    /* each code object can have at most 4G strings */
    if (p >= UINT32_MAX)
        throw Runtime::RuntimeError("Too many names");

    /* add to string table */
    _nameTable.emplace_back(name);
    _names.emplace(name, static_cast<uint32_t>(p));
    return static_cast<uint32_t>(p);
}

uint32_t CodeObject::addLocal(const std::string &name)
{
    /* current constant ID */
    auto it = _locals.find(name);
    size_t p = _localTable.size();

    /* already exists, return the existing ID */
    if (it != _locals.end())
        return it->second;

    /* each code object can have at most 4G strings */
    if (p >= UINT32_MAX)
        throw Runtime::RuntimeError("Too many locals");

    /* add to string table */
    _localTable.emplace_back(name);
    _locals.emplace(name, static_cast<uint32_t>(p));
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

uint32_t CodeObject::emit(int row, int col, Engine::OpCode op)
{
    /* current instruction pointer */
    size_t p = _buffer.size();

    /* each code object is limited to 4G */
    if (p >= UINT32_MAX)
        throw Runtime::RuntimeError("Code exceeds 4G limit");

    /* add to instruction buffer */
    _buffer.emplace_back(static_cast<uint8_t>(op));
    _lineNumTable.emplace_back(row, col);
    return static_cast<uint32_t>(p);
}

uint32_t CodeObject::emitJump(int row, int col, Engine::OpCode op)
{
    /* emit the opcode */
    size_t p = emit(row, col, op);

    /* check for operand space */
    if (p >= UINT32_MAX - sizeof(int32_t) - 1)
        throw Runtime::RuntimeError("Code exceeds 4G limit");

    /* preserve space in instruction buffer */
    _buffer.resize(p + sizeof(int32_t) + 1);
    _lineNumTable.insert(_lineNumTable.end(), sizeof(int32_t), {row, col});
    return static_cast<uint32_t>(p) + 1;
}

uint32_t CodeObject::emitOperand(int row, int col, Engine::OpCode op, int32_t v)
{
    /* emit the opcode */
    char *p = reinterpret_cast<char *>(&v);
    size_t n = emit(row, col, op);

    /* check for operand space */
    if (n >= UINT32_MAX - sizeof(int32_t) - 1)
        throw Runtime::RuntimeError("Code exceeds 4G limit");

    /* add operand to instruction buffer */
    _buffer.insert(_buffer.end(), p, p + sizeof(int32_t));
    _lineNumTable.insert(_lineNumTable.end(), sizeof(int32_t), {row, col});
    return static_cast<uint32_t>(n);
}

void CodeObject::initialize(void)
{
    /* code type object */
    static CodeType _codeType;
    CodeTypeObject = Reference<CodeType>::refStatic(_codeType);
}
}
