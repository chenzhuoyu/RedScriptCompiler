#include "runtime/NullObject.h"

namespace RedScript::Runtime
{
/* type object for null */
TypeRef NullTypeObject;

/* null constant */
ObjectRef NullObject;

void _NullObject::shutdown(void)
{
    NullObject = nullptr;
    NullTypeObject = nullptr;
}

void _NullObject::initialize(void)
{
    /* null type object */
    static NullType nullType;
    NullTypeObject = Reference<NullType>::refStatic(nullType);

    /* null constant */
    static _NullObject nullObject;
    NullObject = Reference<_NullObject>::refStatic(nullObject);
}
}
