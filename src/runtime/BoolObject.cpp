#include "runtime/BoolObject.h"

namespace RedScript::Runtime
{
/* type object for boolean */
TypeRef BoolTypeObject;

/* two static boolean constants */
ObjectRef TrueObject;
ObjectRef FalseObject;

uint64_t BoolType::objectHash(ObjectRef self)
{
    std::hash<bool> hash;
    return hash(self.as<BoolObject>()->_value);
}

std::string BoolType::objectRepr(ObjectRef self)
{
    if (self.as<BoolObject>()->_value)
        return "true";
    else
        return "false";
}

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
    /* boolean type object */
    static BoolType boolType;
    BoolTypeObject = Reference<BoolType>::refStatic(boolType);

    /* boolean constants */
    static BoolObject trueObject(true);
    static BoolObject falseObject(false);
    TrueObject = Reference<BoolObject>::refStatic(trueObject);
    FalseObject = Reference<BoolObject>::refStatic(falseObject);
}
}
