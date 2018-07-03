#include "runtime/NativeFunctionObject.h"
#include "exceptions/InternalError.h"

namespace RedScript::Runtime
{
/* type object for native function */
TypeRef NativeFunctionTypeObject;

ObjectRef NativeFunctionType::objectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs)
{
    /* check object type */
    if (!(self->type().isIdenticalWith(NativeFunctionTypeObject)))
        throw Exceptions::InternalError("Invalid native function call");

    /* convert to function object */
    auto func = self.as<NativeFunctionObject>();
    NativeFunction function = func->function();

    /* check for function instance */
    if (function == nullptr)
        throw Exceptions::InternalError("Empty native function call");

    /* call the native function */
    return function(args, kwargs);
}

void NativeFunctionObject::initialize(void)
{
    /* native function type object */
    static NativeFunctionType nativeFunctionType;
    NativeFunctionTypeObject = Reference<NativeFunctionType>::refStatic(nativeFunctionType);
}
}
