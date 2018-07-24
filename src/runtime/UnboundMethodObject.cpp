#include "runtime/BoundMethodObject.h"
#include "runtime/UnboundMethodObject.h"

namespace RedScript::Runtime
{
/* type object for unbound method */
TypeRef UnboundMethodTypeObject;

void UnboundMethodType::addBuiltins(void)
{
    // TODO: add __invoke__
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

void UnboundMethodObject::initialize(void)
{
    /* unbound method type object */
    static UnboundMethodType unboundMethodType;
    UnboundMethodTypeObject = Reference<UnboundMethodType>::refStatic(unboundMethodType);
}
}
