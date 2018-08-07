#ifndef REDSCRIPT_RUNTIME_MODULEOBJECT_H
#define REDSCRIPT_RUNTIME_MODULEOBJECT_H

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class ModuleType : public NativeType
{
public:
    explicit ModuleType() : NativeType("module") {}

/*** Native Object Protocol ***/

public:
    virtual std::string nativeObjectRepr(ObjectRef self) override;

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

};

/* type object for module */
extern TypeRef ModuleTypeObject;

/* module reference */
class ModuleObject;
typedef Reference<ModuleObject> ModuleRef;

class ModuleObject : public Object
{
    std::string _name;

public:
    virtual ~ModuleObject() { attrs().clear(); }
    explicit ModuleObject(const std::string &name) : Object(ModuleTypeObject), _name(name) {}

public:
    auto &name(void) { return _name; }
    const auto &name(void) const { return _name; }

public:
    static ModuleRef import(const std::string &name);
    static ModuleRef importFrom(const std::string &name, ObjectRef from);

public:
    static void flush(void);
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_MODULEOBJECT_H */
