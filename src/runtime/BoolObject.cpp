#include "runtime/BoolObject.h"

namespace RedScript::Runtime
{
/* type object for boolean */
TypeRef BoolTypeObject;

/* two static boolean constants */
ObjectRef TrueObject;
ObjectRef FalseObject;

/*** Native Object Protocol ***/

uint64_t BoolType::nativeObjectHash(ObjectRef self)
{
    std::hash<bool> hash;
    return hash(self.as<BoolObject>()->_value);
}

std::string BoolType::nativeObjectRepr(ObjectRef self)
{
    if (self.isIdenticalWith(TrueObject))
        return "true";
    else
        return "false";
}

bool BoolType::nativeObjectIsTrue(ObjectRef self)
{
    /* get the bool value */
    return self.isIdenticalWith(TrueObject);
}

ObjectRef BoolObject::fromBool(bool value)
{
    if (value)
        return TrueObject;
    else
        return FalseObject;
}

void BoolObject::shutdown(void)
{
    TrueObject = nullptr;
    FalseObject = nullptr;
    BoolTypeObject = nullptr;
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
