#ifndef REDSCRIPT_MODULES_SYSTEM_H
#define REDSCRIPT_MODULES_SYSTEM_H

#include "runtime/ModuleObject.h"

namespace RedScript::Modules
{
/* sys module object */
extern Runtime::ModuleRef SystemModule;

struct System : public Runtime::ModuleObject
{
    virtual ~System() = default;
    explicit System();

public:
    static void shutdown() { SystemModule = nullptr; }
    static void initialize() { SystemModule = Object::newObject<System>(); }

};
}

#endif /* REDSCRIPT_MODULES_SYSTEM_H */
