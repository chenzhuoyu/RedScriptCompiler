#include <runtime/NullObject.h>
#include "engine/Interpreter.h"
#include "runtime/MapObject.h"
#include "runtime/FunctionObject.h"
#include "exceptions/InternalError.h"

namespace RedScript::Runtime
{
/* type object for function */
TypeRef FunctionTypeObject;

ObjectRef FunctionType::objectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs)
{
    /* check object type */
    if (self->isNotInstanceOf(FunctionTypeObject))
        throw Exceptions::InternalError("Invalid function call");

    /* check tuple type */
    if (args->isNotInstanceOf(TupleTypeObject))
        throw Exceptions::InternalError("Invalid args tuple object");

    /* check map type */
    if (kwargs->isNotInstanceOf(MapTypeObject))
        throw Exceptions::InternalError("Invalid kwargs map object");

    /* call the function handler */
    return self.as<FunctionObject>()->invoke(args.as<TupleObject>(), kwargs.as<MapObject>());
}

ObjectRef FunctionObject::invoke(Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    printf("hey !!!\n");
    return RedScript::Runtime::NullObject;
}

void FunctionObject::initialize(void)
{
    /* function type object */
    static FunctionType functionType;
    FunctionTypeObject = Reference<FunctionType>::refStatic(functionType);
}
}
