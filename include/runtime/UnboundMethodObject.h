#ifndef REDSCRIPT_RUNTIME_UNBOUNDMETHODOBJECT_H
#define REDSCRIPT_RUNTIME_UNBOUNDMETHODOBJECT_H

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
    virtual ObjectRef nativeObjectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs) override;

};

/* type object for unbound method */
extern TypeRef UnboundMethodTypeObject;

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
    static ObjectRef newUnary(UnaryFunction   function) { return fromCallable(NativeFunctionObject::newUnary(function)); }
    static ObjectRef newBinary(BinaryFunction  function) { return fromCallable(NativeFunctionObject::newBinary(function)); }
    static ObjectRef newTernary(TernaryFunction function) { return fromCallable(NativeFunctionObject::newTernary(function)); }
    static ObjectRef newVariadic(NativeFunction  function) { return fromCallable(NativeFunctionObject::newVariadic(function)); }

public:
    template <typename Func>
    static ObjectRef fromFunction(Func &&func) { return fromCallable(NativeFunctionObject::fromFunction(std::forward<Func>(func))); }
    static ObjectRef fromCallable(ObjectRef func) { return Object::newObject<UnboundMethodObject>(func); }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_UNBOUNDMETHODOBJECT_H */
