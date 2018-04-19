#include "runtime/IntObject.h"

namespace RedScript::Runtime
{
/* type object for integer */
TypeRef IntTypeObject;

void IntObject::initialize(void)
{
    static IntType type;
    IntTypeObject = Reference<IntType>::refStatic(type);
}
}
