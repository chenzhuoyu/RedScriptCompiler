#ifndef REDSCRIPT_ENGINE_INTERPRETER_H
#define REDSCRIPT_ENGINE_INTERPRETER_H

#include <memory>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include "utils/Strings.h"
#include "engine/Closure.h"
#include "runtime/Object.h"
#include "runtime/CodeObject.h"
#include "exceptions/InternalError.h"

namespace RedScript::Engine
{
class Interpreter
{
    Runtime::Reference<Runtime::CodeObject> _code;
    const std::unordered_map<std::string, ClosureRef> &_names;

private:
    std::vector<Runtime::ObjectRef> _stack;
    std::vector<Runtime::ObjectRef> _locals;
    std::vector<std::unique_ptr<Closure::Context>> _closures;

private:
    Runtime::ObjectRef tupleConcat(Runtime::ObjectRef a, Runtime::ObjectRef b);
    Runtime::ObjectRef hashmapConcat(Runtime::ObjectRef a, Runtime::ObjectRef b);

public:
    Interpreter(
        Runtime::Reference<Runtime::CodeObject> code,
        const std::unordered_map<std::string, ClosureRef> &names
    ) : _code(code),
        _names(names),
        _locals(code->locals().size()),
        _closures(code->locals().size()) {}

public:
    Runtime::ObjectRef eval(void);

public:
    void setLocal(uint32_t id, Runtime::ObjectRef value)
    {
        /* check for ID range */
        if (id < _locals.size())
            _locals[id] = value;
        else
            throw Exceptions::InternalError(Utils::Strings::format("Invalid local ID %u", id));
    }

public:
    void setLocal(const std::string &name, Runtime::ObjectRef value)
    {
        /* find the local variable ID */
        auto iter = _code->localMap().find(name);

        /* check for existence */
        if (iter != _code->localMap().end())
            _locals[iter->second] = value;
        else
            throw Exceptions::InternalError(Utils::Strings::format("Invalid local name \"%s\"", name));
    }
};
}

#endif /* REDSCRIPT_ENGINE_INTERPRETER_H */
