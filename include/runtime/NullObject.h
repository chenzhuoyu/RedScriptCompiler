#ifndef REDSCRIPT_RUNTIME_NULLOBJECT_H
#define REDSCRIPT_RUNTIME_NULLOBJECT_H

#include <vector>
#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class NullType : public Type
{
public:
    explicit NullType() : Type("null") {}

public:
    virtual bool objectIsTrue(ObjectRef self) override { return false; }

};

/* type object for null */
extern TypeRef NullTypeObject;

class _NullObject : public Object
{
public:
    virtual ~_NullObject() = default;
    explicit _NullObject() : Object(NullTypeObject) {}

public:
    static void shutdown(void) {}
    static void initialize(void);

};

/* null constant */
extern ObjectRef NullObject;
}

#endif /* REDSCRIPT_RUNTIME_NULLOBJECT_H */
