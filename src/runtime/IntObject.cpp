#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"

#include "utils/Strings.h"
#include "exceptions/TypeError.h"

namespace RedScript::Runtime
{
/* type object for integer */
TypeRef IntTypeObject;

uint64_t IntType::objectHash(ObjectRef self)
{
    // TODO: calculate hash
    return std::hash<int64_t>()(self.as<IntObject>()->_value);
}

std::string IntType::objectRepr(ObjectRef self)
{
    // TODO: calculate repr value
    return std::to_string(self.as<IntObject>()->_value);
}

bool IntType::objectIsTrue(ObjectRef self)
{
    // TODO: "0" represents false, otherwise true
    return self.as<IntObject>()->_value != 0;
}

ObjectRef IntType::comparableEq(ObjectRef self, ObjectRef other)
{
    // TODO: implement ==
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, IntTypeObject) &&
        (self.as<IntObject>()->_value == other.as<IntObject>()->_value)
    );
}

ObjectRef IntType::comparableLt(ObjectRef self, ObjectRef other)
{
    // TODO: implement <
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, IntTypeObject) &&
        (self.as<IntObject>()->_value < other.as<IntObject>()->_value)
    );
}

ObjectRef IntType::comparableGt(ObjectRef self, ObjectRef other)
{
    // TODO: implement >
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, IntTypeObject) &&
        (self.as<IntObject>()->_value> other.as<IntObject>()->_value)
    );
}

ObjectRef IntType::comparableNeq(ObjectRef self, ObjectRef other)
{
    // TODO: implement !=
    return BoolObject::fromBool(!(
        other->type()->objectIsInstanceOf(other, IntTypeObject) &&
        (self.as<IntObject>()->_value == other.as<IntObject>()->_value)
    ));
}

ObjectRef IntType::comparableLeq(ObjectRef self, ObjectRef other)
{
    // TODO: implement <=
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, IntTypeObject) &&
        (self.as<IntObject>()->_value <= other.as<IntObject>()->_value)
    );
}

ObjectRef IntType::comparableGeq(ObjectRef self, ObjectRef other)
{
    // TODO: implement >=
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, IntTypeObject) &&
        (self.as<IntObject>()->_value >= other.as<IntObject>()->_value)
    );
}

ObjectRef IntType::comparableCompare(ObjectRef self, ObjectRef other)
{
    if (!other->type()->objectIsInstanceOf(other, IntTypeObject))
        throw Exceptions::TypeError(Utils::Strings::format("\"%s\" is not comparable with \"int\"", other->type()->name()));

    // TODO: implement cmp
    return IntObject::fromInt(
        self.as<IntObject>()->_value > other.as<IntObject>()->_value ? 1 :
        self.as<IntObject>()->_value < other.as<IntObject>()->_value ? -1 : 0
    );
}

bool IntObject::isSafeInt(void)
{
    // TODO: high precision integer
    return _value <= INT32_MAX;
}

bool IntObject::isSafeUInt(void)
{
    // TODO: high precision integer
    return _value >= 0;
}

int64_t IntObject::toInt(void)
{
    // TODO: high precision integer
    return _value;
}

uint64_t IntObject::toUInt(void)
{
    // TODO: high precision integer
    return static_cast<uint64_t>(_value);
}

ObjectRef IntObject::fromInt(int64_t value)
{
    // TODO: implement an integer pool
    return Object::newObject<IntObject>(value);
}

ObjectRef IntObject::fromUInt(uint64_t value)
{
    // TODO: implement an integer pool
    return Object::newObject<IntObject>(value);
}

ObjectRef IntObject::fromString(const std::string &value)
{
    // TODO: convert `value` to `IntObject`
    throw std::runtime_error("not implemented");
}

void IntObject::initialize(void)
{
    /* integer type object */
    static IntType intType;
    IntTypeObject = Reference<IntType>::refStatic(intType);
}
}
