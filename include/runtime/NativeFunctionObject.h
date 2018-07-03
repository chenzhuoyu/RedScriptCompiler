#ifndef REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H
#define REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H

#include <string>
#include <functional>

#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "runtime/TupleObject.h"

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

/* native function types */
typedef Reference<MapObject> KeywordArgs;
typedef Reference<TupleObject> VariadicArgs;
typedef std::function<ObjectRef(VariadicArgs, KeywordArgs)> NativeFunction;

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

public:
    static ObjectRef newVariadic(NativeFunction function)
    {
        /* custom variadic function */
        return Object::newObject<NativeFunctionObject>(function);
    }
};
}

#endif /* REDSCRIPT_RUNTIME_NATIVEFUNCTIONOBJECT_H */
