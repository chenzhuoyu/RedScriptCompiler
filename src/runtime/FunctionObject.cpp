#include "runtime/MapObject.h"
#include "runtime/FunctionObject.h"

namespace RedScript::Runtime
{
/* type object for function */
TypeRef FunctionTypeObject;

ObjectRef FunctionType::objectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs)
{
    printf("self: %s\n", self->type()->objectRepr(self).c_str());
    printf("args: %s (%zu)\n", args->type()->objectRepr(args).c_str(), args.as<Runtime::TupleObject>()->size());
    printf("kwargs: %s (%zu)\n", kwargs->type()->objectRepr(kwargs).c_str(), kwargs.as<Runtime::MapObject>()->size());
    return Type::objectInvoke(self, args, kwargs);
}

void FunctionObject::initialize(void)
{
    /* function type object */
    static FunctionType functionType;
    FunctionTypeObject = Reference<FunctionType>::refStatic(functionType);
}
}
