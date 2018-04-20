#ifndef REDSCRIPT_COMPILER_CODEOBJECT_H
#define REDSCRIPT_COMPILER_CODEOBJECT_H

#include <vector>
#include <string>
#include <cstdint>

#include "engine/Bytecode.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class CodeType : public Type
{
public:
    using Type::Type;

};

/* type object for code */
extern TypeRef CodeTypeObject;

class CodeObject : public Object
{
    friend class CodeType;

private:
    std::vector<char> _buffer;
    std::vector<std::string> _nameTable;
    std::vector<Runtime::ObjectRef> _constTable;

private:
    std::unordered_map<std::string, uint32_t> _names;
    std::unordered_map<std::string, uint32_t> _locals;
    std::unordered_map<Runtime::ObjectRef, uint32_t> _consts;

public:
    virtual ~CodeObject() = default;
    explicit CodeObject() : Object(CodeTypeObject) {}

public:
    const std::vector<char> &buffer(void) const { return _buffer; }
    const std::vector<std::string> &names(void) const { return _nameTable; }
    const std::vector<Runtime::ObjectRef> &consts(void) const { return _constTable; }

public:
    uint32_t emit(Engine::OpCode op);
    uint32_t addConst(Runtime::ObjectRef value);
    uint32_t addLocal(const std::string &value);
    uint32_t addString(const std::string &value);

public:
    uint32_t emitJump(Engine::OpCode op);
    uint32_t emitOperand(Engine::OpCode op, int32_t operand);

public:
    bool isLocal(const std::string &value) const { return _locals.find(value) != _locals.end(); }
    void patchJump(size_t offset, size_t address) { *(int32_t *)(&_buffer[offset]) = static_cast<int32_t>(address - offset + 1); }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_COMPILER_CODEOBJECT_H */
