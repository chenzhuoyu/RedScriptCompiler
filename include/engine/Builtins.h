#ifndef REDSCRIPT_ENGINE_BUILTINS_H
#define REDSCRIPT_ENGINE_BUILTINS_H

#include <string>
#include <unordered_map>

#include "utils/NFI.h"
#include "engine/Closure.h"
#include "runtime/Object.h"
#include "runtime/NativeFunctionObject.h"

namespace RedScript::Engine
{
class Builtins
{
    static Runtime::ObjectRef print(Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs);
    static Runtime::ObjectRef getattr(Runtime::ObjectRef self, const std::string &name, Runtime::ObjectRef def);

public:
    static std::unordered_map<std::string, ClosureRef> Globals;

public:
    static void shutdown(void);
    static void initialize(void);

};
}

#endif /* REDSCRIPT_ENGINE_BUILTINS_H */
