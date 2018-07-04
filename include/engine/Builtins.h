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
    static Runtime::ObjectRef dir(Runtime::ObjectRef obj);
    static Runtime::ObjectRef len(Runtime::ObjectRef obj);
    static Runtime::ObjectRef hash(Runtime::ObjectRef obj);
    static Runtime::ObjectRef repr(Runtime::ObjectRef obj);
    static Runtime::ObjectRef print(Runtime::VariadicArgs args, Runtime::KeywordArgs kwargs);

public:
    static std::unordered_map<std::string, ClosureRef> Globals;

public:
    static void shutdown(void);
    static void initialize(void);

};
}

#endif /* REDSCRIPT_ENGINE_BUILTINS_H */
