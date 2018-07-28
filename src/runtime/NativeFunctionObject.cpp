#include "runtime/MapObject.h"
#include "runtime/TupleObject.h"
#include "runtime/UnboundMethodObject.h"
#include "runtime/NativeFunctionObject.h"

namespace RedScript::Runtime
{
/* type object for native function */
TypeRef NativeFunctionTypeObject;

void NativeFunctionType::addBuiltins(void)
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

ObjectRef NativeFunctionType::nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* check object type */
    if (self->isNotInstanceOf(NativeFunctionTypeObject))
        throw Exceptions::InternalError("Invalid native function call");

    /* convert to function object */
    auto func = self.as<NativeFunctionObject>();
    NativeFunction function = func->function();

    /* check for function instance */
    if (function == nullptr)
        throw Exceptions::InternalError("Empty native function call");

    /* call the native function */
    return function(std::move(args), std::move(kwargs));
}

ObjectRef NativeFunctionObject::newNullary(NullaryFunction function)
{
    return newVariadic([=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
    {
        /* no variadic arguments acceptable */
        if (args->size())
            throw Exceptions::TypeError(Utils::Strings::format("Function takes no arguments, but %zu given", args->size()));

        /* no keyword arguments acceptable */
        if (kwargs->size())
            throw Exceptions::TypeError(Utils::Strings::format("Function takes no keyword arguments, but %zu given", kwargs->size()));

        /* invoke the function */
        return function();
    });
}

ObjectRef NativeFunctionObject::newUnary(UnaryFunction function)
{
    return newVariadic([=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
    {
        /* only 1 argument acceptable */
        if (args->size() != 1)
            throw Exceptions::TypeError(Utils::Strings::format("Function takes exact 1 argument, but %zu given", args->size()));

        /* no keyword arguments acceptable */
        if (kwargs->size())
            throw Exceptions::TypeError(Utils::Strings::format("Function takes no keyword arguments, but %zu given", kwargs->size()));

        /* invoke the function */
        return function(args->items()[0]);
    });
}

ObjectRef NativeFunctionObject::newBinary(BinaryFunction function)
{
    return newVariadic([=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
    {
        /* only 2 arguments acceptable */
        if (args->size() != 2)
            throw Exceptions::TypeError(Utils::Strings::format("Function takes exact 2 arguments, but %zu given", args->size()));

        /* no keyword arguments acceptable */
        if (kwargs->size())
            throw Exceptions::TypeError(Utils::Strings::format("Function takes no keyword arguments, but %zu given", kwargs->size()));

        /* invoke the function */
        return function(args->items()[0], args->items()[1]);
    });
}

ObjectRef NativeFunctionObject::newTernary(TernaryFunction function)
{
    return newVariadic([=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
    {
        /* only 3 arguments acceptable */
        if (args->size() != 3)
            throw Exceptions::TypeError(Utils::Strings::format("Function takes exact 3 arguments, but %zu given", args->size()));

        /* no keyword arguments acceptable */
        if (kwargs->size())
            throw Exceptions::TypeError(Utils::Strings::format("Function takes no keyword arguments, but %zu given", kwargs->size()));

        /* invoke the function */
        return function(args->items()[0], args->items()[1], args->items()[2]);
    });
}

void NativeFunctionObject::shutdown(void)
{
    /* clear type instance */
    NativeFunctionTypeObject = nullptr;
}

void NativeFunctionObject::initialize(void)
{
    /* native function type object */
    static NativeFunctionType nativeFunctionType;
    NativeFunctionTypeObject = Reference<NativeFunctionType>::refStatic(nativeFunctionType);
}
}
