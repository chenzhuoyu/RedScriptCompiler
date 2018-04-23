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
    explicit CodeType() : Type("_CodeObject") {}

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
    std::vector<std::pair<int, int>> _lineNumTable;

private:
    std::unordered_map<std::string, uint32_t> _names;
    std::unordered_map<std::string, uint32_t> _locals;
    std::unordered_map<Runtime::ObjectRef, uint32_t> _consts;

public:
    virtual ~CodeObject() = default;
    explicit CodeObject() : Object(CodeTypeObject) {}

public:
    std::vector<char> &buffer(void) { return _buffer; }
    std::vector<std::string> &names(void) { return _nameTable; }
    std::vector<Runtime::ObjectRef> &consts(void) { return _constTable; }
    std::vector<std::pair<int, int>> &lineNums(void) { return _lineNumTable; }

public:
    uint32_t addConst(Runtime::ObjectRef value);
    uint32_t addLocal(const std::string &value);
    uint32_t addString(const std::string &value);

public:
    uint32_t emit(int row, int col, Engine::OpCode op);
    uint32_t emitJump(int row, int col, Engine::OpCode op);
    uint32_t emitOperand(int row, int col, Engine::OpCode op, int32_t operand);

public:
    bool isLocal(const std::string &value) { return _locals.find(value) != _locals.end(); }
    void patchBranch(uint32_t offset, uint32_t address) { *(uint32_t *)(&_buffer[offset]) = address - offset + 1; }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_COMPILER_CODEOBJECT_H */
