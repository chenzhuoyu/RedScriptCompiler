#include "runtime/NullObject.h"

namespace RedScript::Runtime
{
/* type object for boolean */
TypeRef NullTypeObject;

/* null constant */
ObjectRef NullObject;

void _NullObject::initialize(void)
{
    /* null type and constant */
    NullTypeObject = Reference<NullType>::newStatic("null");
    NullObject = Reference<_NullObject>::newStatic();
}
}
