#ifndef REDSCRIPT_ENGINE_INTERPRETER_H
#define REDSCRIPT_ENGINE_INTERPRETER_H

#include <memory>
#include <vector>
#include <unordered_map>

#include "engine/Closure.h"
#include "runtime/Object.h"
#include "runtime/CodeObject.h"

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
    std::vector<Runtime::ObjectRef> &locals(void) { return _locals; }

};
}

#endif /* REDSCRIPT_ENGINE_INTERPRETER_H */
