#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/DecimalObject.h"
#include "runtime/UnboundMethodObject.h"

#include "exceptions/TypeError.h"
#include "exceptions/InternalError.h"

namespace RedScript::Runtime
{
/* type object for decimal */
TypeRef DecimalTypeObject;

#define UM_UNARY(func)  UnboundMethodObject::newUnary([](ObjectRef self){ return func; })
#define UM_BINARY(func) UnboundMethodObject::newBinary([](ObjectRef self, ObjectRef other){ return func; })

#define ADD_UNARY(name, func)  attrs().emplace(#name, UM_UNARY(self->type()->native ## func(self)))
#define ADD_BINARY(name, func) attrs().emplace(#name, UM_BINARY(self->type()->native ## func(self, other)))

void DecimalType::addBuiltins(void)
{
    /* unary operators */
    ADD_UNARY(__pos__, NumericPos);
    ADD_UNARY(__neg__, NumericNeg);

    /* binary arithmetic operators */
    ADD_BINARY(__add__  , NumericAdd  );
    ADD_BINARY(__sub__  , NumericSub  );
    ADD_BINARY(__mul__  , NumericMul  );
    ADD_BINARY(__div__  , NumericDiv  );
    ADD_BINARY(__mod__  , NumericMod  );
    ADD_BINARY(__power__, NumericPower);

    /* binary comparsion operators */
    ADD_BINARY(__eq__     , ComparableEq     );
    ADD_BINARY(__lt__     , ComparableLt     );
    ADD_BINARY(__gt__     , ComparableGt     );
    ADD_BINARY(__neq__    , ComparableNeq    );
    ADD_BINARY(__leq__    , ComparableLeq    );
    ADD_BINARY(__geq__    , ComparableGeq    );
    ADD_BINARY(__compare__, ComparableCompare);
}

#undef UM_UNARY
#undef UM_BINARY

#undef ADD_UNARY
#undef ADD_BINARY

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
        return Utils::Decimal(other.as<IntObject>()->value());

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

void DecimalObject::shutdown(void)
{
    /* shutdown decimal */
    DecimalTypeObject = nullptr;
    Utils::Decimal::shutdown();
}

void DecimalObject::initialize(void)
{
    /* initialize decimal */
    Utils::Decimal::initialize();

    /* decimal type object */
    static DecimalType decimalType;
    DecimalTypeObject = Reference<DecimalType>::refStatic(decimalType);
}
}
