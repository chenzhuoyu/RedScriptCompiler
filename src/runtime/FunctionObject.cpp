#include "runtime/FunctionObject.h"
#include "exceptions/InternalError.h"

namespace RedScript::Runtime
{
/* type object for function */
TypeRef FunctionTypeObject;

FunctionObject::FunctionObject(ObjectRef code, ObjectRef defaults) : Object(FunctionTypeObject)
{
    /* check for code object */
    if (!(code->type().isIdenticalWith(CodeTypeObject)))
        throw Exceptions::InternalError("`code` must be a code object");

    /* check for tuple object */
    if (!(defaults->type().isIdenticalWith(TupleTypeObject)))
        throw Exceptions::InternalError("`defaults` must be a tuple object");

    /* convert to corresponding type */
    _code = code.as<CodeObject>();
    _defaults = defaults.as<TupleObject>();
}

void FunctionObject::initialize(void)
{
    /* function type object */
    static FunctionType functionType;
    FunctionTypeObject = Reference<FunctionType>::refStatic(functionType);
}
}
