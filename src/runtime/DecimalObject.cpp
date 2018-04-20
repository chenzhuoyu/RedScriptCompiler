#include "runtime/DecimalObject.h"

namespace RedScript::Runtime
{
/* type object for decimal */
TypeRef DecimalTypeObject;

bool DecimalType::objectIsTrue(ObjectRef self)
{
    // TODO: "0.0" represents false, otherwise true
    return false;
}

void DecimalObject::initialize(void)
{
    /* decimal type */
    DecimalTypeObject = Reference<DecimalType>::newStatic("decimal");
}
}
