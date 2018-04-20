#include "runtime/IntObject.h"

namespace RedScript::Runtime
{
/* type object for integer */
TypeRef IntTypeObject;

bool IntType::objectIsTrue(ObjectRef self)
{
    // TODO: "0" represents false, otherwise true
    return false;
}

ObjectRef IntObject::fromInt(int64_t value)
{
    // TODO: convert `value` to `IntObject`
    return RedScript::Runtime::ObjectRef();
}

ObjectRef IntObject::fromString(const std::string &value)
{
    // TODO: convert `value` to `IntObject`
    return RedScript::Runtime::ObjectRef();
}

void IntObject::initialize(void)
{
    /* integer type object */
    static IntType intType;
    IntTypeObject = Reference<IntType>::refStatic(intType);
}
}
