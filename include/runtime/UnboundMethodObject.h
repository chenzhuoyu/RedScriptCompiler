#ifndef REDSCRIPT_RUNTIME_UNBOUNDMETHODOBJECT_H
#define REDSCRIPT_RUNTIME_UNBOUNDMETHODOBJECT_H

#include <functional>

#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "runtime/TupleObject.h"
#include "runtime/NativeFunctionObject.h"

namespace RedScript::Runtime
{
class UnboundMethodType : public Type
{
public:
    explicit UnboundMethodType() : Type("unbound_method") {}

protected:
    virtual void addBuiltins(void) override;
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual ObjectRef nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

/* type object for unbound method */
extern TypeRef UnboundMethodTypeObject;
typedef std::function<ObjectRef(ObjectRef, Reference<TupleObject>, Reference<MapObject>)> UnboundVariadicFunction;

class UnboundMethodObject : public Object
{
    ObjectRef _func;

public:
    virtual ~UnboundMethodObject() = default;
    explicit UnboundMethodObject(ObjectRef func) : Object(UnboundMethodTypeObject), _func(func) { attrs().emplace("um_func", _func); }

public:
    bool isNative(void) { return _func->isInstanceOf(NativeFunctionTypeObject); }
    bool isUserDefined(void) { return _func->isNotInstanceOf(NativeFunctionTypeObject); }

public:
    ObjectRef bind(ObjectRef self);
    ObjectRef invoke(Reference<TupleObject> args, Reference<MapObject> kwargs);

public:
    static ObjectRef newUnary(UnaryFunction function) { return fromCallableObject(NativeFunctionObject::newUnary(function)); }
    static ObjectRef newBinary(BinaryFunction function) { return fromCallableObject(NativeFunctionObject::newBinary(function)); }
    static ObjectRef newTernary(TernaryFunction function) { return fromCallableObject(NativeFunctionObject::newTernary(function)); }
    static ObjectRef newVariadic(NativeFunction function) { return fromCallableObject(NativeFunctionObject::newVariadic(function)); }
    static ObjectRef newUnboundVariadic(UnboundVariadicFunction function);

public:
    template <typename ... Args>
    static ObjectRef fromFunction(Args && ... args) { return fromCallableObject(NativeFunctionObject::fromFunction(std::forward<Args>(args) ...)); }
    static ObjectRef fromCallableObject(ObjectRef func) { return Object::newObject<UnboundMethodObject>(func); }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_UNBOUNDMETHODOBJECT_H */
