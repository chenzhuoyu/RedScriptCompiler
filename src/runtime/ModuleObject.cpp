#include <unordered_map>

#include "utils/RWLock.h"
#include "utils/Strings.h"
#include "engine/Builtins.h"
#include "runtime/ModuleObject.h"
#include "runtime/ExceptionObject.h"

namespace RedScript::Runtime
{
/* type object for module */
TypeRef ModuleTypeObject;

/* global module cache */
static Utils::RWLock _lock;
static std::unordered_map<std::string, ModuleRef> _cache;

std::string ModuleType::nativeObjectRepr(ObjectRef self)
{
    return Utils::Strings::format(
        "<module \"%s\" at %p>",
        self.as<ModuleObject>()->name(),
        static_cast<void *>(self.get())
    );
}

ModuleRef ModuleObject::import(const std::string &name)
{
    using Engine::Builtins;
    decltype(_cache.find(name)) iter;

    /* use cached module if possible */
    {
        Utils::RWLock::Read _(_lock);
        if ((iter = _cache.find(name)) != _cache.end()) return iter->second;
    }

    /* check for built-in modules */
    if ((iter = Builtins::Modules.find(name)) != Builtins::Modules.end())
        return iter->second;

    // TODO: implement import name
    throw Exceptions::InternalError("not implemented: import name");
}

ModuleRef ModuleObject::importFrom(const std::string &name, ObjectRef from)
{
    // TODO: implement import name from
    throw Exceptions::InternalError("not implemented: import name from");
}

void ModuleObject::flush(void)
{
    /* clear module cache */
    Utils::RWLock::Write _(_lock);
    _cache.clear();
}

void ModuleObject::initialize(void)
{
    /* module type object */
    static ModuleType moduleType;
    ModuleTypeObject = Reference<ModuleType>::refStatic(moduleType);
}
}
