#include "runtime/SliceObject.h"

namespace RedScript::Runtime
{
/* type object for slice */
TypeRef SliceTypeObject;

void SliceObject::initialize(void)
{
    /* slice type object */
    static SliceType sliceType;
    SliceTypeObject = Reference<SliceType>::refStatic(sliceType);
}
}
