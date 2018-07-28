#ifndef REDSCRIPT_RUNTIME_CODEOBJECT_H
#define REDSCRIPT_RUNTIME_CODEOBJECT_H

#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "engine/Bytecode.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class CodeType : public NativeType
{
public:
    explicit CodeType() : NativeType("code") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

};

/* type object for code */
extern TypeRef CodeTypeObject;

class CodeObject : public Object
{
    friend class CodeType;

private:
    std::string _file;
    std::string _vargs;
    std::string _kwargs;

private:
    std::vector<char> _buffer;
    std::vector<std::string> _args;
    std::vector<std::string> _names;
    std::vector<std::string> _locals;
    std::vector<Runtime::ObjectRef> _consts;
    std::vector<std::pair<int, int>> _lineNums;

private:
    std::unordered_set<std::string> _freeVars;
    std::unordered_map<std::string, uint32_t> _nameMap;
    std::unordered_map<std::string, uint32_t> _localMap;
    std::unordered_map<Runtime::ObjectRef, uint32_t> _constMap;

public:
    virtual ~CodeObject() = default;
    explicit CodeObject(const std::string &file) : Object(CodeTypeObject), _file(file) {}

public:
    const std::string &file(void) const { return _file; }
    const std::string &vargs(void) const { return _vargs; }
    const std::string &kwargs(void) const { return _kwargs; }

public:
    std::vector<char> &buffer(void) { return _buffer; }
    std::vector<std::string> &args(void) { return _args; }
    std::vector<std::string> &names(void) { return _names; }
    std::vector<std::string> &locals(void) { return _locals; }
    std::vector<Runtime::ObjectRef> &consts(void) { return _consts; }
    std::vector<std::pair<int, int>> &lineNums(void) { return _lineNums; }

public:
    std::unordered_set<std::string> &freeVars(void) { return _freeVars; }
    std::unordered_map<std::string, uint32_t> &nameMap(void) { return _nameMap; }
    std::unordered_map<std::string, uint32_t> &localMap(void) { return _localMap; }

public:
    void setVargs(const std::string &vargs) { _vargs = vargs; }
    void setKwargs(const std::string &kwargs) { _kwargs = kwargs; }
    void markFreeVar(const std::string &freeVar) { _freeVars.emplace(freeVar); }

public:
    uint32_t addName(const std::string &name);
    uint32_t addLocal(const std::string &name);
    uint32_t addConst(Runtime::ObjectRef value);

public:
    uint32_t emit(int row, int col, Engine::OpCode op);
    uint32_t emitJump(int row, int col, Engine::OpCode op);
    uint32_t emitOperand(int row, int col, Engine::OpCode op, int32_t v);
    uint32_t emitOperand2(int row, int col, Engine::OpCode op, int32_t v1, int32_t v2);

public:
    bool isLocal(const std::string &value) const { return _localMap.find(value) != _localMap.end(); }
    void patchBranch(uint32_t offset, uint32_t address) { *(uint32_t *)(&_buffer[offset]) = address - offset + 1; }

public:
    static void shutdown(void);
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_CODEOBJECT_H */
