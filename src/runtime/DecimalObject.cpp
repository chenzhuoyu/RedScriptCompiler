#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/DecimalObject.h"

#include "exceptions/TypeError.h"
#include "exceptions/InternalError.h"

namespace RedScript::Runtime
{
/* type object for decimal */
TypeRef DecimalTypeObject;

bool DecimalType::compare(ObjectRef self, ObjectRef other, bool (*ret)(int))
{
    if (other->isInstanceOf(DecimalTypeObject))
        return ret(self.as<DecimalObject>()->_value.cmp(other.as<DecimalObject>()->_value));
    else if (other->isInstanceOf(IntTypeObject))
        return ret(self.as<DecimalObject>()->_value.cmp(Utils::Decimal(other.as<IntObject>()->value())));
    else
        return false;
}

Utils::Decimal DecimalType::toDecimal(ObjectRef other)
{
    /* decimal objects */
    if (other->isInstanceOf(DecimalTypeObject))
        return other.as<DecimalObject>()->_value;

    /* integer objects */
    else if (other->isInstanceOf(IntTypeObject))
        return Utils::Decimal(other.as<IntObject>()->value().toString());

    /* otherwise it's an error */
    else
        throw Exceptions::TypeError("Operand must be `int` or `decimal` object");
}

/*** Object Protocol ***/

uint64_t    DecimalType::objectHash  (ObjectRef self) { return self.as<DecimalObject>()->_value.toHash();    }
std::string DecimalType::objectRepr  (ObjectRef self) { return self.as<DecimalObject>()->_value.toString();  }
bool        DecimalType::objectIsTrue(ObjectRef self) { return !(self.as<DecimalObject>()->_value.isZero()); }

/*** Numeric Protocol ***/

ObjectRef DecimalType::numericPos(ObjectRef self) { return DecimalObject::fromDecimal(+(self.as<DecimalObject>()->_value)); }
ObjectRef DecimalType::numericNeg(ObjectRef self) { return DecimalObject::fromDecimal(-(self.as<DecimalObject>()->_value)); }

ObjectRef DecimalType::numericAdd  (ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value  +  toDecimal(other));  }
ObjectRef DecimalType::numericSub  (ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value  -  toDecimal(other));  }
ObjectRef DecimalType::numericMul  (ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value  *  toDecimal(other));  }
ObjectRef DecimalType::numericDiv  (ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value  /  toDecimal(other));  }
ObjectRef DecimalType::numericMod  (ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value  %  toDecimal(other));  }
ObjectRef DecimalType::numericPower(ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value.pow(toDecimal(other))); }

/*** Comparator Protocol ***/

ObjectRef DecimalType::comparableEq     (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret == 0; })); }
ObjectRef DecimalType::comparableLt     (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret <  0; })); }
ObjectRef DecimalType::comparableGt     (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret >  0; })); }
ObjectRef DecimalType::comparableNeq    (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret != 0; })); }
ObjectRef DecimalType::comparableLeq    (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret <= 0; })); }
ObjectRef DecimalType::comparableGeq    (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret >= 0; })); }
ObjectRef DecimalType::comparableCompare(ObjectRef self, ObjectRef other) { return IntObject::fromInt(self.as<DecimalObject>()->_value.cmp(toDecimal(other)));  }

void DecimalObject::initialize(void)
{
    /* decimal type object */
    static DecimalType decimalType;
    DecimalTypeObject = Reference<DecimalType>::refStatic(decimalType);
}
}
