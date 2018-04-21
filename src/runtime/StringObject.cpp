#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/StringObject.h"

#include "utils/Strings.h"
#include "runtime/TypeError.h"

namespace RedScript::Runtime
{
/* type object for string */
TypeRef StringTypeObject;

uint64_t StringType::objectHash(ObjectRef self)
{
    static std::hash<std::string> hash;
    return hash(self.as<StringObject>()->_value);
}

bool StringType::objectIsTrue(ObjectRef self)
{
    /* non-empty string represents "true" */
    return !self.as<StringObject>()->_value.empty();
}

std::string StringType::objectStr(ObjectRef self)
{
    /* just give the string itself */
    return self.as<StringObject>()->_value;
}

std::string StringType::objectRepr(ObjectRef self)
{
    const auto &str = self.as<StringObject>()->_value;
    return Utils::Strings::repr(str.data(), str.size());
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
        throw TypeError(Utils::Strings::format("\"%s\" is not comparable with \"str\"", other->type()->name()));
    else
        return IntObject::fromInt(self.as<StringObject>()->_value.compare(other.as<StringObject>()->_value));
}

ObjectRef StringObject::fromString(const std::string &value)
{
    // TODO: implement a string pool
    return Object::newObject<StringObject>(value);
}

void StringObject::initialize(void)
{
    /* string type object */
    static StringType stringType;
    StringTypeObject = Reference<StringType>::refStatic(stringType);
}
}
