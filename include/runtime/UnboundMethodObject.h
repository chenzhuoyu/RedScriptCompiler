#ifndef REDSCRIPT_RUNTIME_UNBOUNDMETHODOBJECT_H
#define REDSCRIPT_RUNTIME_UNBOUNDMETHODOBJECT_H

#include <functional>

#include "runtime/Object.h"
#include "runtime/MapObject.h"
#include "runtime/TupleObject.h"
#include "runtime/NativeFunctionObject.h"

namespace RedScript::Runtime
{
class UnboundMethodType : public NativeType
{
public:
    explicit UnboundMethodType() : NativeType("unbound_method") {}

protected:
    virtual void addBuiltins(void) override;
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual std::string nativeObjectRepr(ObjectRef self) override;
    virtual ObjectRef   nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs) override;

};

/* type object for unbound method */
extern TypeRef UnboundMethodTypeObject;
typedef std::function<ObjectRef(ObjectRef, Reference<TupleObject>, Reference<MapObject>)> UnboundVariadicFunction;

class UnboundMethodObject : public Object
{
    ObjectRef _func;
    std::string _name;

public:
    virtual ~UnboundMethodObject() = default;
    explicit UnboundMethodObject(const std::string &name, ObjectRef func);

public:
    bool isNative(void) { return _func->isInstanceOf(NativeFunctionTypeObject); }
    bool isUserDefined(void) { return _func->isNotInstanceOf(NativeFunctionTypeObject); }

public:
    ObjectRef &func(void) { return _func; }
    std::string &name(void) { return _name; }

public:
    ObjectRef bind(ObjectRef self);
    ObjectRef invoke(Reference<TupleObject> args, Reference<MapObject> kwargs);

public:
    static Reference<UnboundMethodObject> newUnary(const std::string &name, UnaryFunction function) { return fromCallable(name, NativeFunctionObject::newUnary(name, function)); }
    static Reference<UnboundMethodObject> newBinary(const std::string &name, BinaryFunction function) { return fromCallable(name, NativeFunctionObject::newBinary(name, function)); }
    static Reference<UnboundMethodObject> newTernary(const std::string &name, TernaryFunction function) { return fromCallable(name, NativeFunctionObject::newTernary(name, function)); }
    static Reference<UnboundMethodObject> newVariadic(const std::string &name, NativeFunction function) { return fromCallable(name, NativeFunctionObject::newVariadic(name, function)); }
    static Reference<UnboundMethodObject> newUnboundVariadic(const std::string &name, UnboundVariadicFunction function);

public:
    static Reference<UnboundMethodObject> fromCallable(ObjectRef func);
    static Reference<UnboundMethodObject> fromCallable(const std::string &name, ObjectRef func) { return Object::newObject<UnboundMethodObject>(name, func); }

public:
    template <typename ... Args>
    static Reference<UnboundMethodObject> fromFunction(const std::string &name, Args && ... args)
    {
        return fromCallable(
            name,
            NativeFunctionObject::fromFunction(
                name,
                std::forward<Args>(args) ...
            )
        );
    }

public:
    static void shutdown(void);
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_UNBOUNDMETHODOBJECT_H */
