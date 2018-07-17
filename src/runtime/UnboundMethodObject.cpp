#include "runtime/BoundMethodObject.h"
#include "runtime/UnboundMethodObject.h"

namespace RedScript::Runtime
{
/* type object for unbound method */
TypeRef UnboundMethodTypeObject;

ObjectRef UnboundMethodType::objectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs)
{
    /* check object type */
    if (self->isNotInstanceOf(UnboundMethodTypeObject))
        throw Exceptions::InternalError("Invalid unbound method call");

    /* check tuple type */
    if (args->isNotInstanceOf(TupleTypeObject))
        throw Exceptions::InternalError("Invalid args tuple object");

    /* check map type */
    if (kwargs->isNotInstanceOf(MapTypeObject))
        throw Exceptions::InternalError("Invalid kwargs map object");

    /* call the method handler */
    return self.as<UnboundMethodObject>()->invoke(args.as<TupleObject>(), kwargs.as<MapObject>());
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

void UnboundMethodObject::initialize(void)
{
    /* unbound method type object */
    static UnboundMethodType unboundMethodType;
    UnboundMethodTypeObject = Reference<UnboundMethodType>::refStatic(unboundMethodType);
}
}
