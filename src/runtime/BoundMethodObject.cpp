#include "runtime/BoundMethodObject.h"
#include "runtime/UnboundMethodObject.h"

namespace RedScript::Runtime
{
/* type object for bound method */
TypeRef BoundMethodTypeObject;

void BoundMethodType::addBuiltins(void)
{
    // TODO: add __invoke__
}

/*** Native Object Protocol ***/

ObjectRef BoundMethodType::nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    if (self->isNotInstanceOf(BoundMethodTypeObject))
        throw Exceptions::InternalError("Invalid bound method call");
    else
        return self.as<BoundMethodObject>()->invoke(std::move(args), std::move(kwargs));
}

BoundMethodObject::BoundMethodObject(ObjectRef self, ObjectRef func) :
    Object(BoundMethodTypeObject),
    _self(self),
    _func(func)
{
    attrs().emplace("bm_func", _func);
    attrs().emplace("bm_self", _self);
}

ObjectRef BoundMethodObject::invoke(Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    /* one more item (self argument) */
    size_t argc = args->size();
    Reference<TupleObject> tuple = TupleObject::fromSize(argc + 1);

    /* fill all other arguments */
    for (size_t i = 0; i < argc; i++)
        tuple->items()[i + 1] = args->items()[i];

    /* bind `self` to the first argument */
    tuple->items()[0] = _self;

    /* invoke the function with `self` bound */
    return _func->type()->objectInvoke(_func, std::move(tuple), std::move(kwargs));
}

void BoundMethodObject::initialize(void)
{
    /* bound method type object */
    static BoundMethodType boundMethodType;
    BoundMethodTypeObject = Reference<BoundMethodType>::refStatic(boundMethodType);
}
}
