#include "utils/Strings.h"
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
    addMethod(UnboundMethodObject::newUnboundVariadic(
        "__invoke__",
        [](ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs){ return self->type()->objectInvoke(self, args, kwargs); }
    ));
}

/*** Native Object Protocol ***/

std::string NativeFunctionType::nativeObjectRepr(ObjectRef self)
{
    return Utils::Strings::format(
        "<native function \"%s\" at %p>",
        self.as<NativeFunctionObject>()->name(),
        static_cast<void *>(self.get())
    );
}

ObjectRef NativeFunctionType::nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    if (self->isNotInstanceOf(NativeFunctionTypeObject))
        throw Exceptions::InternalError("Invalid native function call");
    else
        return self.as<NativeFunctionObject>()->function()(std::move(args), std::move(kwargs));
}

FunctionRef NativeFunctionObject::newNullary(const std::string &name, NullaryFunction function)
{
    return newVariadic(name, [=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
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

FunctionRef NativeFunctionObject::newUnary(const std::string &name, UnaryFunction function)
{
    return newVariadic(name, [=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
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

FunctionRef NativeFunctionObject::newBinary(const std::string &name, BinaryFunction function)
{
    return newVariadic(name, [=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
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

FunctionRef NativeFunctionObject::newTernary(const std::string &name, TernaryFunction function)
{
    return newVariadic(name, [=](Utils::NFI::VariadicArgs args, Utils::NFI::KeywordArgs kwargs)
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
