#include "runtime/DecimalObject.h"
#include "exceptions/InternalError.h"

namespace RedScript::Runtime
{
/* type object for decimal */
TypeRef DecimalTypeObject;

uint64_t    DecimalType::objectHash  (ObjectRef self) { return self.as<DecimalObject>()->_value.toHash();    }
std::string DecimalType::objectRepr  (ObjectRef self) { return self.as<DecimalObject>()->_value.toString();  }
bool        DecimalType::objectIsTrue(ObjectRef self) { return !(self.as<DecimalObject>()->_value.isZero()); }

ObjectRef DecimalType::comparableEq(ObjectRef self, ObjectRef other)
{
    return Type::comparableEq(self, other);
}

ObjectRef DecimalType::comparableLt(ObjectRef self, ObjectRef other)
{
    return Type::comparableLt(self, other);
}

ObjectRef DecimalType::comparableGt(ObjectRef self, ObjectRef other)
{
    return Type::comparableGt(self, other);
}

ObjectRef DecimalType::comparableNeq(ObjectRef self, ObjectRef other)
{
    return Type::comparableNeq(self, other);
}

ObjectRef DecimalType::comparableLeq(ObjectRef self, ObjectRef other)
{
    return Type::comparableLeq(self, other);
}

ObjectRef DecimalType::comparableGeq(ObjectRef self, ObjectRef other)
{
    return Type::comparableGeq(self, other);
}

ObjectRef DecimalType::comparableCompare(ObjectRef self, ObjectRef other)
{
    return Type::comparableCompare(self, other);
}

void DecimalObject::initialize(void)
{
    /* decimal type object */
    static DecimalType decimalType;
    DecimalTypeObject = Reference<DecimalType>::refStatic(decimalType);
}
}
