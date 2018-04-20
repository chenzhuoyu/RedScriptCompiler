#include "runtime/BoolObject.h"

namespace RedScript::Runtime
{
/* type object for boolean */
TypeRef BoolTypeObject;

/* two static boolean constants */
ObjectRef TrueObject;
ObjectRef FalseObject;

bool BoolType::objectIsTrue(ObjectRef self)
{
    /* get the bool value */
    return self.as<BoolObject>()->_value;
}

ObjectRef BoolObject::fromBool(bool value)
{
    if (value)
        return TrueObject;
    else
        return FalseObject;
}

void BoolObject::initialize(void)
{
    /* boolean type */
    BoolTypeObject = Reference<BoolType>::newStatic("bool");

    /* boolean constants */
    TrueObject = Reference<BoolObject>::newStatic(true);
    FalseObject = Reference<BoolObject>::newStatic(false);
}
}
