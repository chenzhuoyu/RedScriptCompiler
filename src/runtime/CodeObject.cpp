#include "runtime/CodeObject.h"
#include "exceptions/RuntimeError.h"

namespace RedScript::Runtime
{
/* type object for code */
TypeRef CodeTypeObject;

uint32_t CodeObject::addName(const std::string &name)
{
    /* current name ID */
    auto it = _nameMap.find(name);
    size_t id = _names.size();

    /* already exists, return the existing ID */
    if (it != _nameMap.end())
        return it->second;

    /* each code object can have at most 4G names */
    if (id >= UINT32_MAX)
        throw Exceptions::RuntimeError("Too many names");

    /* add to name table */
    _names.emplace_back(name);
    _nameMap.emplace(name, static_cast<uint32_t>(id));
    return static_cast<uint32_t>(id);
}

uint32_t CodeObject::addLocal(const std::string &name)
{
    /* current local ID */
    auto it = _localMap.find(name);
    size_t id = _locals.size();

    /* already exists, return the existing ID */
    if (it != _localMap.end())
        return it->second;

    /* each code object can have at most 4G locals */
    if (id >= UINT32_MAX)
        throw Exceptions::RuntimeError("Too many locals");

    /* add to string table */
    _locals.emplace_back(name);
    _localMap.emplace(name, static_cast<uint32_t>(id));
    return static_cast<uint32_t>(id);
}

uint32_t CodeObject::addConst(Runtime::ObjectRef value)
{
    /* current constant ID */
    auto it = _constMap.find(value);
    size_t id = _consts.size();

    /* already exists, return the existing ID */
    if (it != _constMap.end())
        return it->second;

    /* each code object can have at most 4G constants */
    if (id >= UINT32_MAX)
        throw Exceptions::RuntimeError("Too many constants");

    /* add to constant table */
    _consts.emplace_back(value);
    _constMap.emplace(value, static_cast<uint32_t>(id));
    return static_cast<uint32_t>(id);
}

uint32_t CodeObject::emit(int row, int col, Engine::OpCode op)
{
    /* current instruction pointer */
    size_t pc = _buffer.size();

    /* each code object is limited to 4G */
    if (pc >= UINT32_MAX)
        throw Exceptions::RuntimeError("Code exceeds 4G limit");

    /* add to instruction buffer */
    _buffer.emplace_back(static_cast<uint8_t>(op));
    _lineNums.emplace_back(row, col);
    return static_cast<uint32_t>(pc);
}

uint32_t CodeObject::emitJump(int row, int col, Engine::OpCode op)
{
    /* emit the opcode */
    size_t pc = emit(row, col, op);

    /* check for operand space */
    if (pc >= UINT32_MAX - sizeof(int32_t) - 1)
        throw Exceptions::RuntimeError("Code exceeds 4G limit");

    /* preserve space in instruction buffer */
    _buffer.resize(pc + sizeof(int32_t) + 1);
    _lineNums.insert(_lineNums.end(), sizeof(int32_t), {row, col});
    return static_cast<uint32_t>(pc) + 1;
}

uint32_t CodeObject::emitOperand(int row, int col, Engine::OpCode op, int32_t v)
{
    /* emit the opcode */
    char *p = reinterpret_cast<char *>(&v);
    size_t pc = emit(row, col, op);

    /* check for operand space */
    if (pc >= UINT32_MAX - sizeof(int32_t) - 1)
        throw Exceptions::RuntimeError("Code exceeds 4G limit");

    /* add operand to instruction buffer */
    _buffer.insert(_buffer.end(), p, p + sizeof(int32_t));
    _lineNums.insert(_lineNums.end(), sizeof(int32_t), {row, col});
    return static_cast<uint32_t>(pc);
}

uint32_t CodeObject::emitOperand2(int row, int col, Engine::OpCode op, int32_t v1, int32_t v2)
{
    /* emit the opcode */
    char *p1 = reinterpret_cast<char *>(&v1);
    char *p2 = reinterpret_cast<char *>(&v2);
    size_t pc = emit(row, col, op);

    /* check for operand space */
    if (pc >= UINT32_MAX - sizeof(int32_t) * 2 - 1)
        throw Exceptions::RuntimeError("Code exceeds 4G limit");

    /* add operand to instruction buffer */
    _buffer.insert(_buffer.end(), p1, p1 + sizeof(int32_t));
    _buffer.insert(_buffer.end(), p2, p2 + sizeof(int32_t));
    _lineNums.insert(_lineNums.end(), sizeof(int32_t) * 2, {row, col});
    return static_cast<uint32_t>(pc);
}

void CodeObject::initialize(void)
{
    /* code type object */
    static CodeType _codeType;
    CodeTypeObject = Reference<CodeType>::refStatic(_codeType);
}
}
