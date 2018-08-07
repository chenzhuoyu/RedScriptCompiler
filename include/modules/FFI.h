#ifndef REDSCRIPT_MODULES_FFI_H
#define REDSCRIPT_MODULES_FFI_H

#include "runtime/ModuleObject.h"

namespace RedScript::Modules
{
/* FFI module object */
extern Runtime::ModuleRef FFIModule;

struct FFI : public Runtime::ModuleObject
{
    virtual ~FFI() = default;
    explicit FFI();

public:
    static void shutdown() { FFIModule = nullptr; }
    static void initialize() { FFIModule = Object::newObject<FFI>(); }

};
}

#endif /* REDSCRIPT_MODULES_FFI_H */
