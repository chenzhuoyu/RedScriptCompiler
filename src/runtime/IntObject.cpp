#include <array>

#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/DecimalObject.h"

#include "utils/Strings.h"
#include "exceptions/TypeError.h"
#include "exceptions/ValueError.h"

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

/*** Object Protocol ***/

uint64_t    IntType::objectHash  (ObjectRef self) { return self.as<IntObject>()->_value.toHash();    }
std::string IntType::objectRepr  (ObjectRef self) { return self.as<IntObject>()->_value.toString();  }
bool        IntType::objectIsTrue(ObjectRef self) { return !(self.as<IntObject>()->_value.isZero()); }

/*** Numeric Protocol ***/

ObjectRef IntType::numericPos(ObjectRef self) { return IntObject::fromInteger(+(self.as<IntObject>()->value())); }
ObjectRef IntType::numericNeg(ObjectRef self) { return IntObject::fromInteger(-(self.as<IntObject>()->value())); }
ObjectRef IntType::numericNot(ObjectRef self) { return IntObject::fromInteger(~(self.as<IntObject>()->value())); }

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

ObjectRef IntType::numericAdd(ObjectRef self, ObjectRef other) NUM_OP_F(+)
ObjectRef IntType::numericSub(ObjectRef self, ObjectRef other) NUM_OP_F(-)
ObjectRef IntType::numericMul(ObjectRef self, ObjectRef other) NUM_OP_F(*)
ObjectRef IntType::numericDiv(ObjectRef self, ObjectRef other) NUM_OP_F(/)
ObjectRef IntType::numericMod(ObjectRef self, ObjectRef other) NUM_OP_F(%)

ObjectRef IntType::numericPower(ObjectRef self, ObjectRef other)
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

ObjectRef IntType::numericOr    (ObjectRef self, ObjectRef other) NUM_OP_I(|)
ObjectRef IntType::numericAnd   (ObjectRef self, ObjectRef other) NUM_OP_I(&)
ObjectRef IntType::numericXor   (ObjectRef self, ObjectRef other) NUM_OP_I(^)
ObjectRef IntType::numericLShift(ObjectRef self, ObjectRef other) NUM_OP_I(<<)
ObjectRef IntType::numericRShift(ObjectRef self, ObjectRef other) NUM_OP_I(>>)

/*** Comparator Protocol ***/

ObjectRef IntType::comparableEq (ObjectRef self, ObjectRef other) BOOL_OP_F(==)
ObjectRef IntType::comparableLt (ObjectRef self, ObjectRef other) BOOL_OP_F(<)
ObjectRef IntType::comparableGt (ObjectRef self, ObjectRef other) BOOL_OP_F(>)
ObjectRef IntType::comparableNeq(ObjectRef self, ObjectRef other) BOOL_OP_F(!=)
ObjectRef IntType::comparableLeq(ObjectRef self, ObjectRef other) BOOL_OP_F(<=)
ObjectRef IntType::comparableGeq(ObjectRef self, ObjectRef other) BOOL_OP_F(>=)

ObjectRef IntType::comparableCompare(ObjectRef self, ObjectRef other)
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

ObjectRef IntObject::fromInteger(const Utils::Integer &value)
{
    /* read from integer pool if within range */
    if (value.isSafeInt())
        return fromInt(value.toInt());
    else
        return Object::newObject<IntObject>(value);
}

void IntObject::shutdown(void)
{
    /* clear all integers in pool */
    for (auto &item : _pool)
        item = nullptr;
}

void IntObject::initialize(void)
{
    /* integer type object */
    static IntType intType;
    IntTypeObject = Reference<IntType>::refStatic(intType);

    /* initialize integer pool */
    for (int64_t i = POOL_LOWER; i <= POOL_UPPER; i++)
        _pool[i - POOL_LOWER] = Object::newObject<IntObject>(i);
}
}
