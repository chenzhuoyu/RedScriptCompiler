#include "runtime/NullObject.h"
#include "runtime/NativeFunctionObject.h"

#include "exceptions/TypeError.h"
#include "exceptions/InternalError.h"

namespace RedScript::Runtime
{
/* type object for native function */
TypeRef NativeFunctionTypeObject;

ObjectRef NativeFunctionType::objectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs)
{
    /* check object type */
    if (self->isNotInstanceOf(NativeFunctionTypeObject))
        throw Exceptions::InternalError("Invalid native function call");

    /* check tuple type */
    if (args->isNotInstanceOf(TupleTypeObject))
        throw Exceptions::InternalError("Invalid tuple object");

    /* check map type */
    if (kwargs->isNotInstanceOf(MapTypeObject))
        throw Exceptions::InternalError("Invalid map object");

    /* convert to function object */
    auto func = self.as<NativeFunctionObject>();
    NativeFunction function = func->function();

    /* check for function instance */
    if (function == nullptr)
        throw Exceptions::InternalError("Empty native function call");

    /* call the native function */
    return function(args.as<TupleObject>(), kwargs.as<MapObject>());
}

ObjectRef NativeFunctionObject::newNullary(NullaryFunction func)
{
    return newVariadic([=](VariadicArgs args, KeywordArgs kwargs)
    {
        /* no variadic arguments acceptable */
        if (args->size())
            throw Exceptions::TypeError(Utils::Strings::format("Function takes no arguments, but %zu given", args->size()));

        /* no keyword arguments acceptable */
        if (kwargs->size())
            throw Exceptions::TypeError(Utils::Strings::format("Function takes no keyword arguments, but %zu given", kwargs->size()));

        /* invoke the function */
        return func();
    });
}

ObjectRef NativeFunctionObject::newUnary(UnaryFunction func)
{
    return newVariadic([=](VariadicArgs args, KeywordArgs kwargs)
    {
        /* only 1 argument acceptable */
        if (args->size() != 1)
            throw Exceptions::TypeError(Utils::Strings::format("Function takes exact 1 argument, but %zu given", args->size()));

        /* no keyword arguments acceptable */
        if (kwargs->size())
            throw Exceptions::TypeError(Utils::Strings::format("Function takes no keyword arguments, but %zu given", kwargs->size()));

        /* invoke the function */
        return func(args->items()[0]);
    });
}

ObjectRef NativeFunctionObject::newBinary(BinaryFunction func)
{
    return newVariadic([=](VariadicArgs args, KeywordArgs kwargs)
    {
        /* only 2 arguments acceptable */
        if (args->size() != 2)
            throw Exceptions::TypeError(Utils::Strings::format("Function takes exact 2 arguments, but %zu given", args->size()));

        /* no keyword arguments acceptable */
        if (kwargs->size())
            throw Exceptions::TypeError(Utils::Strings::format("Function takes no keyword arguments, but %zu given", kwargs->size()));

        /* invoke the function */
        return func(args->items()[0], args->items()[1]);
    });
}

ObjectRef NativeFunctionObject::newTernary(TernaryFunction func)
{
    return newVariadic([=](VariadicArgs args, KeywordArgs kwargs)
    {
        /* only 3 arguments acceptable */
        if (args->size() != 3)
            throw Exceptions::TypeError(Utils::Strings::format("Function takes exact 3 arguments, but %zu given", args->size()));

        /* no keyword arguments acceptable */
        if (kwargs->size())
            throw Exceptions::TypeError(Utils::Strings::format("Function takes no keyword arguments, but %zu given", kwargs->size()));

        /* invoke the function */
        return func(args->items()[0], args->items()[1], args->items()[2]);
    });
}

void NativeFunctionObject::initialize(void)
{
    /* native function type object */
    static NativeFunctionType nativeFunctionType;
    NativeFunctionTypeObject = Reference<NativeFunctionType>::refStatic(nativeFunctionType);
}
}
