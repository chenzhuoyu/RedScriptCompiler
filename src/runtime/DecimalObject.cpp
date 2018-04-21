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

ObjectRef DecimalObject::fromDouble(double value)
{
    // TODO: high precision floating point
    return Object::newObject<DecimalObject>(value);
}

ObjectRef DecimalObject::fromString(const std::string &value)
{
    // TODO: high precision floating point
    throw std::runtime_error("not implemented");
}

void DecimalObject::initialize(void)
{
    /* decimal type object */
    static DecimalType decimalType;
    DecimalTypeObject = Reference<DecimalType>::refStatic(decimalType);
}
}
