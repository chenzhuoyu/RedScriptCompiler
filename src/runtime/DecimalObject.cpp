#include "runtime/DecimalObject.h"

namespace RedScript::Runtime
{
/* type object for decimal */
TypeRef DecimalTypeObject;

void DecimalObject::initialize(void)
{
    static DecimalType type;
    DecimalTypeObject = Reference<DecimalType>::refStatic(type);
}
}
