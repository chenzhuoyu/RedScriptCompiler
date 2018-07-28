#include "runtime/BoundMethodObject.h"
#include "runtime/UnboundMethodObject.h"

#include "utils/NFI.h"
#include "exceptions/TypeError.h"

namespace RedScript::Runtime
{
/* type object for unbound method */
TypeRef UnboundMethodTypeObject;

void UnboundMethodType::addBuiltins(void)
{
    attrs().emplace(
        "__invoke__",
        UnboundMethodObject::newUnboundVariadic([](ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
        {
            /* invoke the object protocol */
            return self->type()->objectInvoke(self, args, kwargs);
        })
    );
}

/*** Native Object Protocol ***/

ObjectRef UnboundMethodType::nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    if (self->isNotInstanceOf(UnboundMethodTypeObject))
        throw Exceptions::InternalError("Invalid unbound method call");
    else
        return self.as<UnboundMethodObject>()->invoke(std::move(args), std::move(kwargs));
}

ObjectRef UnboundMethodObject::bind(ObjectRef self)
{
    /* wrap as bound method */
    return Object::newObject<BoundMethodObject>(std::move(self), _func);
}

ObjectRef UnboundMethodObject::invoke(Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* invoke the function without binding */
    return _func->type()->objectInvoke(_func, std::move(args), std::move(kwargs));
}

ObjectRef UnboundMethodObject::newUnboundVariadic(UnboundVariadicFunction function)
{
    /* wrap as variadic function, extract the first argument as `self` */
    return newVariadic([=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
    {
        /* try pop from keyword arguments */
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
