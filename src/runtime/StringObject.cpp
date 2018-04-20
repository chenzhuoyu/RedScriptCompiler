#include "utils/Strings.h"
#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/StringObject.h"
#include "runtime/TypeError.h"

namespace RedScript::Runtime
{
/* type object for string */
TypeRef StringTypeObject;

bool StringType::objectIsTrue(ObjectRef self)
{
    /* non-empty string represents "true" */
    return !self.as<StringObject>()->_value.empty();
}

ObjectRef StringType::comparableEq(ObjectRef self, ObjectRef other)
{
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, StringTypeObject) &&
        (self.as<StringObject>()->_value == other.as<StringObject>()->_value)
    );
}

ObjectRef StringType::comparableLe(ObjectRef self, ObjectRef other)
{
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, StringTypeObject) &&
        (self.as<StringObject>()->_value < other.as<StringObject>()->_value)
    );
}

ObjectRef StringType::comparableGe(ObjectRef self, ObjectRef other)
{
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, StringTypeObject) &&
        (self.as<StringObject>()->_value > other.as<StringObject>()->_value)
    );
}

ObjectRef StringType::comparableNeq(ObjectRef self, ObjectRef other)
{
    return BoolObject::fromBool(!(
        other->type()->objectIsInstanceOf(other, StringTypeObject) &&
        (self.as<StringObject>()->_value == other.as<StringObject>()->_value)
    ));
}

ObjectRef StringType::comparableLeq(ObjectRef self, ObjectRef other)
{
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, StringTypeObject) &&
        (self.as<StringObject>()->_value <= other.as<StringObject>()->_value)
    );
}

ObjectRef StringType::comparableGeq(ObjectRef self, ObjectRef other)
{
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, StringTypeObject) &&
        (self.as<StringObject>()->_value >= other.as<StringObject>()->_value)
    );
}

ObjectRef StringType::comparableCompare(ObjectRef self, ObjectRef other)
{
    if (!other->type()->objectIsInstanceOf(other, StringTypeObject))
        throw TypeError(Utils::Strings::format("\"%s\" is not comparable with \"string\"", other->type()->name()));
    else
        return IntObject::fromInt(self.as<StringObject>()->_value.compare(other.as<StringObject>()->_value));
}

void StringObject::initialize(void)
{
    /* string type */
    StringTypeObject = Reference<StringType>::newStatic("string");
}
}
