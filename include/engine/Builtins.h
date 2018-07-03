#ifndef REDSCRIPT_ENGINE_BUILTINS_H
#define REDSCRIPT_ENGINE_BUILTINS_H

#include <string>
#include <unordered_map>

#include "engine/Closure.h"
#include "runtime/Object.h"
#include "runtime/NativeFunctionObject.h"

namespace RedScript::Engine
{
class Builtins
{
    static Runtime::ObjectRef print(Runtime::VariadicArgs args, Runtime::KeywordArgs kwargs);

public:
    typedef std::unordered_map<std::string, ClosureRef> BuiltinClosure;

public:
    static BuiltinClosure &closure(void);

};
}

#endif /* REDSCRIPT_ENGINE_BUILTINS_H */
