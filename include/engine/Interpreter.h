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
    std::string _name;
    std::vector<Runtime::ObjectRef> _stack;
    std::vector<Runtime::ObjectRef> _locals;
    std::vector<std::unique_ptr<Closure::Context>> _closures;

private:
    Runtime::ObjectRef tupleConcat(Runtime::ObjectRef a, Runtime::ObjectRef b);
    Runtime::ObjectRef hashmapConcat(Runtime::ObjectRef a, Runtime::ObjectRef b);
    std::unordered_map<std::string, Engine::ClosureRef> closureCreate(Runtime::Reference<Runtime::CodeObject> &code);

public:
    Interpreter(
        const std::string &name,
        Runtime::Reference<Runtime::CodeObject> code,
        const std::unordered_map<std::string, ClosureRef> &names) : _name(name), _code(code), _names(names)
    {
        _stack.reserve(1024);
        _locals.resize(code->locals().size());
        _closures.resize(code->locals().size());
    }

public:
    Runtime::ObjectRef eval(void);
    Runtime::ObjectRef &locals(size_t id) { return _locals.at(id); }
    Runtime::ObjectRef &locals(const std::string &name) { return _locals[_code->localMap().at(name)]; }

};
}

#endif /* REDSCRIPT_ENGINE_INTERPRETER_H */
