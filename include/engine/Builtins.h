#ifndef REDSCRIPT_ENGINE_BUILTINS_H
#define REDSCRIPT_ENGINE_BUILTINS_H

#include <string>
#include <unordered_map>

#include "utils/NFI.h"
#include "engine/Closure.h"
#include "runtime/Object.h"
#include "runtime/ModuleObject.h"
#include "runtime/NativeFunctionObject.h"

namespace RedScript::Engine
{
class Builtins
{
    static Runtime::ObjectRef print(Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs);
    static Runtime::ObjectRef getattr(Runtime::ObjectRef self, const std::string &name, Runtime::ObjectRef def);

public:
    static std::unordered_map<std::string, ClosureRef> Globals;
    static std::unordered_map<std::string, Runtime::ModuleRef> Modules;

private:
    static void addObject(const char *name, Runtime::ObjectRef object) { Globals.emplace(name, Closure::ref(std::move(object))); }
    static void addObject(const std::string &name, Runtime::ObjectRef object) { Globals.emplace(name, Closure::ref(std::move(object))); }

private:
    static void addModule(Runtime::ModuleRef module) { Modules.emplace(module->name(), module); }
    static void addFunction(Runtime::FunctionRef function) { addObject(function->name(), function); }

public:
    static void shutdown(void);
    static void initialize(void);

};
}

#endif /* REDSCRIPT_ENGINE_BUILTINS_H */
