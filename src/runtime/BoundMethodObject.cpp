#include "utils/Strings.h"
#include "runtime/BoundMethodObject.h"
#include "runtime/UnboundMethodObject.h"

namespace RedScript::Runtime
{
/* type object for bound method */
TypeRef BoundMethodTypeObject;

void BoundMethodType::addBuiltins(void)
{
    addMethod(UnboundMethodObject::newUnboundVariadic(
        "__invoke__",
        [](ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs){ return self->type()->objectInvoke(self, args, kwargs); }
    ));
}

/*** Native Object Protocol ***/

std::string BoundMethodType::nativeObjectRepr(ObjectRef self)
{
    return Utils::Strings::format(
        "<bound method \"%s\" at %p>",
        self.as<BoundMethodObject>()->name(),
        static_cast<void *>(self.get())
    );
}

ObjectRef BoundMethodType::nativeObjectInvoke(ObjectRef self, Reference<TupleObject> args, Reference<MapObject> kwargs)
{
    if (self->isNotInstanceOf(BoundMethodTypeObject))
        throw Exceptions::InternalError("Invalid bound method call");
    else
        return self.as<BoundMethodObject>()->invoke(std::move(args), std::move(kwargs));
}

BoundMethodObject::BoundMethodObject(const std::string &name, ObjectRef self, ObjectRef func) :
    Object(BoundMethodTypeObject),
    _name(name),
    _self(self),
    _func(func)
{
    addObject("bm_func", _func);
    addObject("bm_self", _self);
    addObject("bm_name", StringObject::fromStringInterned(name));
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

void BoundMethodObject::shutdown(void)
{
    /* clear type instance */
    BoundMethodTypeObject = nullptr;
}

void BoundMethodObject::initialize(void)
{
    /* bound method type object */
    static BoundMethodType boundMethodType;
    BoundMethodTypeObject = Reference<BoundMethodType>::refStatic(boundMethodType);
}
}
