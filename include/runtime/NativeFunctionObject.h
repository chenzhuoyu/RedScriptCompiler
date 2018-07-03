#ifndef REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H
#define REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H

#include <string>
#include <functional>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class NativeFunctionType : public Type
{
public:
    explicit NativeFunctionType() : Type("native_function") {}

public:
    virtual ObjectRef objectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs) override;

};

/* type object for native function */
extern TypeRef NativeFunctionTypeObject;
typedef std::function<ObjectRef(ObjectRef, ObjectRef)> NativeFunction;

class NativeFunctionObject : public Object
{
    NativeFunction _function;

public:
    virtual ~NativeFunctionObject() = default;
    explicit NativeFunctionObject(NativeFunction function) : Object(NativeFunctionTypeObject), _function(function) {}

public:
    NativeFunction function(void) const { return _function; }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H */
