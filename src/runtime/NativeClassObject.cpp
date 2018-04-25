#include "runtime/NativeClassObject.h"

namespace RedScript::Runtime
{
/* type object for native class */
TypeRef NativeClassTypeObject;

void NativeClassObject::initialize(void)
{
    /* native class type object */
    static NativeClassType nullType;
    NativeClassTypeObject = Reference<NativeClassType>::refStatic(nullType);
}
}
