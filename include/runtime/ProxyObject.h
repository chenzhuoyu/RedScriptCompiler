#ifndef REDSCRIPT_RUNTIME_DESCRIPTOROBJECT_H
#define REDSCRIPT_RUNTIME_DESCRIPTOROBJECT_H

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class ProxyType : public ObjectType
{
public:
    explicit ProxyType() : ObjectType("proxy") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

};

/* type object for proxy */
extern TypeRef ProxyTypeObject;

class ProxyObject : public Object
{
    ObjectRef _getter;
    ObjectRef _setter;
    ObjectRef _deleter;

public:
    virtual ~ProxyObject() = default;
    explicit ProxyObject(ObjectRef getter, ObjectRef setter, ObjectRef deleter) :
        Object(ProxyTypeObject),
        _getter(getter),
        _setter(setter),
        _deleter(deleter) {}

public:
    ObjectRef getter(void) { return _getter; }
    ObjectRef setter(void) { return _setter; }
    ObjectRef deleter(void) { return _deleter; }

public:
    static ObjectRef newReadOnly(ObjectRef getter) { return Object::newObject<ProxyObject>(getter, nullptr, nullptr); }
    static ObjectRef newReadWrite(ObjectRef getter, ObjectRef setter) { return Object::newObject<ProxyObject>(getter, setter, nullptr); }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_BOOLOBJECT_H */
