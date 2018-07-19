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

/*** Native Object Protocol ***/

uint64_t    DecimalType::nativeObjectHash  (ObjectRef self) { return self.as<DecimalObject>()->_value.toHash();    }
std::string DecimalType::nativeObjectRepr  (ObjectRef self) { return self.as<DecimalObject>()->_value.toString();  }
bool        DecimalType::nativeObjectIsTrue(ObjectRef self) { return !(self.as<DecimalObject>()->_value.isZero()); }

/*** Native Numeric Protocol ***/

ObjectRef DecimalType::nativeNumericPos(ObjectRef self) { return DecimalObject::fromDecimal(+(self.as<DecimalObject>()->_value)); }
ObjectRef DecimalType::nativeNumericNeg(ObjectRef self) { return DecimalObject::fromDecimal(-(self.as<DecimalObject>()->_value)); }

ObjectRef DecimalType::nativeNumericAdd  (ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value  +  toDecimal(other));  }
ObjectRef DecimalType::nativeNumericSub  (ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value  -  toDecimal(other));  }
ObjectRef DecimalType::nativeNumericMul  (ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value  *  toDecimal(other));  }
ObjectRef DecimalType::nativeNumericDiv  (ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value  /  toDecimal(other));  }
ObjectRef DecimalType::nativeNumericMod  (ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value  %  toDecimal(other));  }
ObjectRef DecimalType::nativeNumericPower(ObjectRef self, ObjectRef other) { return DecimalObject::fromDecimal(self.as<DecimalObject>()->_value.pow(toDecimal(other))); }

/*** Native Comparator Protocol ***/

ObjectRef DecimalType::nativeComparableEq     (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret == 0; })); }
ObjectRef DecimalType::nativeComparableLt     (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret <  0; })); }
ObjectRef DecimalType::nativeComparableGt     (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret >  0; })); }
ObjectRef DecimalType::nativeComparableNeq    (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret != 0; })); }
ObjectRef DecimalType::nativeComparableLeq    (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret <= 0; })); }
ObjectRef DecimalType::nativeComparableGeq    (ObjectRef self, ObjectRef other) { return BoolObject::fromBool(compare(self, other, [](int ret){ return ret >= 0; })); }
ObjectRef DecimalType::nativeComparableCompare(ObjectRef self, ObjectRef other) { return IntObject::fromInt(self.as<DecimalObject>()->_value.cmp(toDecimal(other)));  }

void DecimalObject::initialize(void)
{
    /* decimal type object */
    static DecimalType decimalType;
    DecimalTypeObject = Reference<DecimalType>::refStatic(decimalType);
}
}
