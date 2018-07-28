#ifndef REDSCRIPT_RUNTIME_NULLOBJECT_H
#define REDSCRIPT_RUNTIME_NULLOBJECT_H

#include <string>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class NullType : public NativeType
{
public:
    explicit NullType() : NativeType("null") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual bool nativeObjectIsTrue(ObjectRef self) override { return false; }
    virtual std::string nativeObjectRepr(ObjectRef self) override { return "null"; }

};

/* type object for null */
extern TypeRef NullTypeObject;

class _NullObject : public Object
{
public:
    virtual ~_NullObject() = default;
    explicit _NullObject() : Object(NullTypeObject) {}

public:
    static void shutdown(void);
    static void initialize(void);

};

/* null constant */
extern ObjectRef NullObject;
}

#endif /* REDSCRIPT_RUNTIME_NULLOBJECT_H */
