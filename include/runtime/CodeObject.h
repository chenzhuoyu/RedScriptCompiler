#ifndef REDSCRIPT_RUNTIME_CODEOBJECT_H
#define REDSCRIPT_RUNTIME_CODEOBJECT_H

#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

#include "engine/Bytecode.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class CodeType : public Type
{
public:
    explicit CodeType() : Type("code") {}

};

/* type object for code */
extern TypeRef CodeTypeObject;

class CodeObject : public Object
{
    friend class CodeType;

private:
    std::string _vargs;
    std::string _kwargs;

private:
    std::vector<char> _buffer;
    std::vector<std::string> _argTable;
    std::vector<std::string> _nameTable;
    std::vector<std::string> _localTable;
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
    const std::string &vargs(void) const { return _vargs; }
    const std::string &kwargs(void) const { return _kwargs; }

public:
    std::vector<char> &buffer(void) { return _buffer; }
    std::vector<std::string> &args(void) { return _argTable; }
    std::vector<std::string> &names(void) { return _nameTable; }
    std::vector<std::string> &locals(void) { return _localTable; }
    std::vector<Runtime::ObjectRef> &consts(void) { return _constTable; }
    std::vector<std::pair<int, int>> &lineNums(void) { return _lineNumTable; }

public:
    std::unordered_map<std::string, uint32_t> &nameMap(void) { return _names; }
    std::unordered_map<std::string, uint32_t> &localMap(void) { return _locals; }

public:
    void setVargs(const std::string &vargs) { _vargs = vargs; }
    void setKwargs(const std::string &kwargs) { _kwargs = kwargs; }

public:
    uint32_t addName(const std::string &name);
    uint32_t addLocal(const std::string &name);
    uint32_t addConst(Runtime::ObjectRef value);

public:
    uint32_t emit(int row, int col, Engine::OpCode op);
    uint32_t emitJump(int row, int col, Engine::OpCode op);
    uint32_t emitOperand(int row, int col, Engine::OpCode op, int32_t v);

public:
    bool isLocal(const std::string &value) const { return _locals.find(value) != _locals.end(); }
    void patchBranch(uint32_t offset, uint32_t address) { *(uint32_t *)(&_buffer[offset]) = address - offset + 1; }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_CODEOBJECT_H */
