#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"

#include "utils/Strings.h"
#include "exceptions/TypeError.h"
#include "exceptions/ValueError.h"

namespace RedScript::Runtime
{
/* type object for integer */
TypeRef IntTypeObject;

uint64_t    IntType::objectHash  (ObjectRef self) { return self.as<IntObject>()->_value.toHash();    }
std::string IntType::objectRepr  (ObjectRef self) { return self.as<IntObject>()->_value.toString();  }
bool        IntType::objectIsTrue(ObjectRef self) { return !(self.as<IntObject>()->_value.isZero()); }

ObjectRef IntType::comparableEq(ObjectRef self, ObjectRef other)
{
    return Type::comparableEq(self, other);
}

ObjectRef IntType::comparableLt(ObjectRef self, ObjectRef other)
{
    return Type::comparableLt(self, other);
}

ObjectRef IntType::comparableGt(ObjectRef self, ObjectRef other)
{
    return Type::comparableGt(self, other);
}

ObjectRef IntType::comparableNeq(ObjectRef self, ObjectRef other)
{
    return Type::comparableNeq(self, other);
}

ObjectRef IntType::comparableLeq(ObjectRef self, ObjectRef other)
{
    return Type::comparableLeq(self, other);
}

ObjectRef IntType::comparableGeq(ObjectRef self, ObjectRef other)
{
    return Type::comparableGeq(self, other);
}

ObjectRef IntType::comparableCompare(ObjectRef self, ObjectRef other)
{
    return Type::comparableCompare(self, other);
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

void IntObject::initialize(void)
{
    /* integer type object */
    static IntType intType;
    IntTypeObject = Reference<IntType>::refStatic(intType);
}
}
