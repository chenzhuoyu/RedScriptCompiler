#ifndef REDSCRIPT_ENGINE_INTERPRETER_H
#define REDSCRIPT_ENGINE_INTERPRETER_H

#include <unordered_map>

#include "engine/Closure.h"
#include "runtime/Object.h"
#include "runtime/CodeObject.h"

namespace RedScript::Engine
{
class Interpreter
{
    Runtime::ObjectRef tupleConcat(Runtime::ObjectRef a, Runtime::ObjectRef b);
    Runtime::ObjectRef hashmapConcat(Runtime::ObjectRef a, Runtime::ObjectRef b);

public:
    Runtime::ObjectRef eval(
        Runtime::Reference<Runtime::CodeObject> code,
        const std::unordered_map<std::string, ClosureRef> &closure
    );
};
}

#endif /* REDSCRIPT_ENGINE_INTERPRETER_H */
