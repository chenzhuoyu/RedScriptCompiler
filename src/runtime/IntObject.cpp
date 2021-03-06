#include <array>

#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/DecimalObject.h"
#include "runtime/ExceptionObject.h"
#include "runtime/UnboundMethodObject.h"

#include "utils/Decimal.h"

namespace RedScript::Runtime
{
/* type object for integer */
TypeRef IntTypeObject;

/* global integer pool, -32 ~ 255
 * read-only during the life time of the interpreter
 * so no need to make it thread-local or protect by a lock */
static const int64_t POOL_LOWER = -32;
static const int64_t POOL_UPPER = 255;
static std::array<ObjectRef, POOL_UPPER - POOL_LOWER + 1> _pool;

#define ADD_UNARY(name, func)  addMethod(UnboundMethodObject::newUnary(#name, [](ObjectRef self){ return self->type()->native ## func(self); }))
#define ADD_BINARY(name, func) addMethod(UnboundMethodObject::newBinary(#name, [](ObjectRef self, ObjectRef other){ return self->type()->native ## func(self, other); }))

void IntType::addBuiltins(void)
{
    /* unary operators */
    ADD_UNARY(__pos__, NumericPos);
    ADD_UNARY(__neg__, NumericNeg);
    ADD_UNARY(__not__, NumericNot);

    /* binary arithmetic operators */
    ADD_BINARY(__add__  , NumericAdd  );
    ADD_BINARY(__sub__  , NumericSub  );
    ADD_BINARY(__mul__  , NumericMul  );
    ADD_BINARY(__div__  , NumericDiv  );
    ADD_BINARY(__mod__  , NumericMod  );
    ADD_BINARY(__power__, NumericPower);

    /* binary bitwise operators */
    ADD_BINARY(__or__    , NumericOr    );
    ADD_BINARY(__and__   , NumericAnd   );
    ADD_BINARY(__xor__   , NumericXor   );
    ADD_BINARY(__lshift__, NumericLShift);
    ADD_BINARY(__rshift__, NumericRShift);

    /* binary comparsion operators */
    ADD_BINARY(__eq__     , ComparableEq     );
    ADD_BINARY(__lt__     , ComparableLt     );
    ADD_BINARY(__gt__     , ComparableGt     );
    ADD_BINARY(__neq__    , ComparableNeq    );
    ADD_BINARY(__leq__    , ComparableLeq    );
    ADD_BINARY(__geq__    , ComparableGeq    );
    ADD_BINARY(__compare__, ComparableCompare);
}

#undef ADD_UNARY
#undef ADD_BINARY

/*** Native Object Protocol ***/

uint64_t    IntType::nativeObjectHash  (ObjectRef self) { return self.as<IntObject>()->_value.toHash();    }
std::string IntType::nativeObjectRepr  (ObjectRef self) { return self.as<IntObject>()->_value.toString();  }
bool        IntType::nativeObjectIsTrue(ObjectRef self) { return !(self.as<IntObject>()->_value.isZero()); }

/*** Native Numeric Protocol ***/

ObjectRef IntType::nativeNumericPos(ObjectRef self) { return IntObject::fromInteger(+(self.as<IntObject>()->value())); }
ObjectRef IntType::nativeNumericNeg(ObjectRef self) { return IntObject::fromInteger(-(self.as<IntObject>()->value())); }
ObjectRef IntType::nativeNumericNot(ObjectRef self) { return IntObject::fromInteger(~(self.as<IntObject>()->value())); }

#define NUM_OP_I(op) {                                                                                  \
    if (other->isNotInstanceOf(IntTypeObject))                                                          \
        throw Exceptions::TypeError("Operand must be `int` object");                                    \
    else                                                                                                \
        return IntObject::fromInteger(self.as<IntObject>()->_value op other.as<IntObject>()->_value);   \
}

#define NUM_OP_F(op) {                                                                                                          \
    if (other->isInstanceOf(IntTypeObject))                                                                                     \
        return IntObject::fromInteger(self.as<IntObject>()->_value op other.as<IntObject>()->_value);                           \
    else if (other->isInstanceOf(DecimalTypeObject))                                                                            \
        return DecimalObject::fromDecimal(Utils::Decimal(self.as<IntObject>()->_value) op other.as<DecimalObject>()->value());  \
    else                                                                                                                        \
        throw Exceptions::TypeError("Operand must be `int` or `decimal` object");                                               \
}

#define BOOL_OP_F(op) {                                                                                                     \
    if (other->isInstanceOf(IntTypeObject))                                                                                 \
        return BoolObject::fromBool(self.as<IntObject>()->_value op other.as<IntObject>()->_value);                         \
    else if (other->isInstanceOf(DecimalTypeObject))                                                                        \
        return BoolObject::fromBool(Utils::Decimal(self.as<IntObject>()->_value) op other.as<DecimalObject>()->value());    \
    else                                                                                                                    \
        return FalseObject;                                                                                                 \
}

ObjectRef IntType::nativeNumericAdd(ObjectRef self, ObjectRef other) NUM_OP_F(+)
ObjectRef IntType::nativeNumericSub(ObjectRef self, ObjectRef other) NUM_OP_F(-)
ObjectRef IntType::nativeNumericMul(ObjectRef self, ObjectRef other) NUM_OP_F(*)
ObjectRef IntType::nativeNumericDiv(ObjectRef self, ObjectRef other) NUM_OP_F(/)
ObjectRef IntType::nativeNumericMod(ObjectRef self, ObjectRef other) NUM_OP_F(%)

ObjectRef IntType::nativeNumericPower(ObjectRef self, ObjectRef other)
{
    /* int ** decimal */
    if (other->isInstanceOf(DecimalTypeObject))
        return DecimalObject::fromDecimal(Utils::Decimal(self.as<IntObject>()->_value).pow(other.as<DecimalObject>()->value()));

    /* must be integers */
    if (other->isNotInstanceOf(IntTypeObject))
        throw Exceptions::TypeError("Operand must be `int` or `decimal` object");

    /* extract the integer objects */
    auto &val = self.as<IntObject>()->_value;
    auto &exp = other.as<IntObject>()->_value;

    /* use integer power if available, otherwise use decimal power */
    if (exp.isSafeUInt())
        return IntObject::fromInteger(val.pow(exp.toUInt()));
    else
        return DecimalObject::fromDecimal(Utils::Decimal(val).pow(exp).toInt());
}

ObjectRef IntType::nativeNumericOr    (ObjectRef self, ObjectRef other) NUM_OP_I(|)
ObjectRef IntType::nativeNumericAnd   (ObjectRef self, ObjectRef other) NUM_OP_I(&)
ObjectRef IntType::nativeNumericXor   (ObjectRef self, ObjectRef other) NUM_OP_I(^)
ObjectRef IntType::nativeNumericLShift(ObjectRef self, ObjectRef other) NUM_OP_I(<<)
ObjectRef IntType::nativeNumericRShift(ObjectRef self, ObjectRef other) NUM_OP_I(>>)

/*** Native Comparator Protocol ***/

ObjectRef IntType::nativeComparableEq (ObjectRef self, ObjectRef other) BOOL_OP_F(==)
ObjectRef IntType::nativeComparableLt (ObjectRef self, ObjectRef other) BOOL_OP_F(<)
ObjectRef IntType::nativeComparableGt (ObjectRef self, ObjectRef other) BOOL_OP_F(>)
ObjectRef IntType::nativeComparableNeq(ObjectRef self, ObjectRef other) BOOL_OP_F(!=)
ObjectRef IntType::nativeComparableLeq(ObjectRef self, ObjectRef other) BOOL_OP_F(<=)
ObjectRef IntType::nativeComparableGeq(ObjectRef self, ObjectRef other) BOOL_OP_F(>=)

ObjectRef IntType::nativeComparableCompare(ObjectRef self, ObjectRef other)
{
    if (other->isInstanceOf(IntTypeObject))
        return IntObject::fromInt(self.as<IntObject>()->_value.cmp(other.as<IntObject>()->_value));
    else if (other->isInstanceOf(DecimalTypeObject))
        return IntObject::fromInt(Utils::Decimal(self.as<IntObject>()->_value).cmp(other.as<DecimalObject>()->value()));
    else
        throw Exceptions::TypeError("Operand must be `int` or `decimal` object");
}

#undef NUM_OP_I
#undef NUM_OP_F
#undef BOOL_OP_F

ObjectRef IntObject::fromInt(int64_t value)
{
    /* read from integer pool if within range */
    if ((value >= POOL_LOWER) && (value <= POOL_UPPER))
        return _pool[value - POOL_LOWER];
    else
        return Object::newObject<IntObject>(value);
}

ObjectRef IntObject::fromUInt(uint64_t value)
{
    /* read from integer pool if within range */
    if (value <= POOL_UPPER)
        return _pool[value - POOL_LOWER];
    else
        return Object::newObject<IntObject>(value);
}

ObjectRef IntObject::fromInteger(Utils::Integer &&value)
{
    /* definately not within pool range */
    if (!(value.isSafeInt()))
        return Object::newObject<IntObject>(std::move(value));

    /* get it's value and pool index */
    int64_t val = value.toInt();
    int64_t index = val - POOL_LOWER;

    /* read from integer pool if within range */
    if ((val >= POOL_LOWER) && (val <= POOL_UPPER))
        return _pool[index];
    else
        return Object::newObject<IntObject>(std::move(value));
}

ObjectRef IntObject::fromInteger(const Utils::Integer &value)
{
    /* definately not within pool range */
    if (!(value.isSafeInt()))
        return Object::newObject<IntObject>(value);

    /* get it's value and pool index */
    int64_t val = value.toInt();
    int64_t index = val - POOL_LOWER;

    /* read from integer pool if within range */
    if ((val >= POOL_LOWER) && (val <= POOL_UPPER))
        return _pool[index];
    else
        return Object::newObject<IntObject>(value);
}

void IntObject::shutdown(void)
{
    /* clear all integers in pool */
    for (auto &item : _pool)
        item = nullptr;

    /* shutdown integer */
    IntTypeObject = nullptr;
    Utils::Integer::shutdown();
}

void IntObject::initialize(void)
{
    /* initialize integer */
    Utils::Integer::initialize();

    /* integer type object */
    static IntType intType;
    IntTypeObject = Reference<IntType>::refStatic(intType);

    /* initialize integer pool */
    for (int64_t i = POOL_LOWER; i <= POOL_UPPER; i++)
        _pool[i - POOL_LOWER] = Object::newObject<IntObject>(i);
}
}
