#include "runtime/ArrayObject.h"

namespace RedScript::Runtime
{
/* type object for arrays */
TypeRef ArrayTypeObject;

void ArrayObject::initialize(void)
{
    /* array type object */
    static ArrayType arrayType;
    ArrayTypeObject = Reference<ArrayType>::refStatic(arrayType);
}
}
