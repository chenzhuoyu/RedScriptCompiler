#include "runtime/StringObject.h"
#include "runtime/FunctionObject.h"
#include "runtime/ExceptionObject.h"
#include "runtime/BoundMethodObject.h"
#include "runtime/UnboundMethodObject.h"

#include "utils/NFI.h"
#include "utils/Strings.h"

namespace RedScript::Runtime
{
/* type object for unbound method */
TypeRef UnboundMethodTypeObject;

void UnboundMethodType::addBuiltins(void)
{
    addMethod(UnboundMethodObject::newUnboundVariadic(
        "__invoke__",
        [](ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs){ return self->type()->objectInvoke(self, args, kwargs); }
    ));
}

/*** Native Object Protocol ***/

std::string UnboundMethodType::nativeObjectRepr(ObjectRef self)
{
    return Utils::Strings::format(
        "<unbound method \"%s\" at %p>",
        self.as<UnboundMethodObject>()->name(),
        static_cast<void *>(self.get())
    );
}

ObjectRef UnboundMethodType::nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    if (self->isNotInstanceOf(UnboundMethodTypeObject))
        throw Exceptions::InternalError("Invalid unbound method call");
    else
        return self.as<UnboundMethodObject>()->invoke(std::move(args), std::move(kwargs));
}

UnboundMethodObject::UnboundMethodObject(const std::string &name, ObjectRef func) :
    Object(UnboundMethodTypeObject),
    _func(func),
    _name(name)
{
    addObject("um_func", _func);
    addObject("um_name", StringObject::fromStringInterned(_name));
}

ObjectRef UnboundMethodObject::bind(ObjectRef self)
{
    /* wrap as bound method */
    return Object::newObject<BoundMethodObject>(_name, std::move(self), _func);
}

ObjectRef UnboundMethodObject::invoke(Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* invoke the function without binding */
    return _func->type()->objectInvoke(_func, std::move(args), std::move(kwargs));
}

Reference<UnboundMethodObject> UnboundMethodObject::fromCallable(ObjectRef func)
{
    /* classes */
    if (func->isInstanceOf(TypeObject))
        return fromCallable(func.as<Type>()->name(), func);

    /* functions */
    else if (func->isInstanceOf(FunctionTypeObject))
        return fromCallable(func.as<FunctionObject>()->name(), func);

    /* native functions */
    else if (func->isInstanceOf(NativeFunctionTypeObject))
        return fromCallable(func.as<NativeFunctionObject>()->name(), func);

    /* other objects */
    else
        return fromCallable(func->type()->objectRepr(func), func);
}

Reference<UnboundMethodObject> UnboundMethodObject::newUnboundVariadic(const std::string &name, UnboundVariadicFunction function)
{
    /* wrap as variadic function, extract the first argument as `self` */
    return newVariadic(name, [=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
    {
        /* try popping from keyword arguments */
        size_t off = 0;
        ObjectRef self = kwargs->pop(StringObject::fromStringInterned("self"));

        /* if not found, read from positional arguments */
        if (self.isNull() && (args->size() > 1))
        {
            off = 1;
            self = args->items()[0];
        }

        /* must have `self` argument */
        if (self.isNull())
            throw Exceptions::TypeError("Missing \"self\" argument");

        /* extracted argument list */
        size_t size = args->size() - off;
        Reference<TupleObject> argv = TupleObject::fromSize(size);

        /* fill the argument list */
        for (size_t i = 0; i < size; i++)
            argv->items()[i] = args->items()[i + off];

        /* invoke the real function */
        return function(std::move(self), std::move(argv), std::move(kwargs));
    });
}

void UnboundMethodObject::shutdown(void)
{
    /* clear type instance */
    UnboundMethodTypeObject = nullptr;
}

void UnboundMethodObject::initialize(void)
{
    /* unbound method type object */
    static UnboundMethodType unboundMethodType;
    UnboundMethodTypeObject = Reference<UnboundMethodType>::refStatic(unboundMethodType);
}
}
