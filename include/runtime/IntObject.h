#ifndef REDSCRIPT_RUNTIME_INTOBJECT_H
#define REDSCRIPT_RUNTIME_INTOBJECT_H

#include <string>
#include <cstdint>
#include <utils/Decimal.h>

#include "utils/Integer.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class IntType : public NativeType
{
public:
    explicit IntType() : NativeType("int") {}

protected:
    virtual void addBuiltins(void) override;
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual uint64_t    nativeObjectHash(ObjectRef self) override;
    virtual std::string nativeObjectRepr(ObjectRef self) override;

public:
    virtual bool nativeObjectIsTrue(ObjectRef self) override;

/*** Native Numeric Protocol ***/

public:
    virtual ObjectRef nativeNumericPos(ObjectRef self) override;
    virtual ObjectRef nativeNumericNeg(ObjectRef self) override;
    virtual ObjectRef nativeNumericNot(ObjectRef self) override;

public:
    virtual ObjectRef nativeNumericAdd  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericSub  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericMul  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericDiv  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericMod  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericPower(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef nativeNumericOr (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericAnd(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericXor(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef nativeNumericLShift(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericRShift(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef nativeNumericIncAdd  (ObjectRef self, ObjectRef other) override { return nativeNumericAdd  (self, other); }
    virtual ObjectRef nativeNumericIncSub  (ObjectRef self, ObjectRef other) override { return nativeNumericSub  (self, other); }
    virtual ObjectRef nativeNumericIncMul  (ObjectRef self, ObjectRef other) override { return nativeNumericMul  (self, other); }
    virtual ObjectRef nativeNumericIncDiv  (ObjectRef self, ObjectRef other) override { return nativeNumericDiv  (self, other); }
    virtual ObjectRef nativeNumericIncMod  (ObjectRef self, ObjectRef other) override { return nativeNumericMod  (self, other); }
    virtual ObjectRef nativeNumericIncPower(ObjectRef self, ObjectRef other) override { return nativeNumericPower(self, other); }

public:
    virtual ObjectRef nativeNumericIncOr (ObjectRef self, ObjectRef other) override { return nativeNumericOr (self, other); }
    virtual ObjectRef nativeNumericIncAnd(ObjectRef self, ObjectRef other) override { return nativeNumericAnd(self, other); }
    virtual ObjectRef nativeNumericIncXor(ObjectRef self, ObjectRef other) override { return nativeNumericXor(self, other); }

public:
    virtual ObjectRef nativeNumericIncLShift(ObjectRef self, ObjectRef other) override { return nativeNumericLShift(self, other); }
    virtual ObjectRef nativeNumericIncRShift(ObjectRef self, ObjectRef other) override { return nativeNumericRShift(self, other); }

/*** Native Comparator Protocol ***/

public:
    virtual ObjectRef nativeComparableEq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableLt(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableGt(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableNeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableLeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableGeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableCompare(ObjectRef self, ObjectRef other) override;

};

/* type object for integer */
extern TypeRef IntTypeObject;

class IntObject : public Object
{
    Utils::Integer _value;
    friend class IntType;

public:
    virtual ~IntObject() = default;
    explicit IntObject(Utils::Integer &&value)      : Object(IntTypeObject), _value(std::move(value)) {}
    explicit IntObject(const Utils::Integer &value) : Object(IntTypeObject), _value(value) {}

public:
    const Utils::Integer &value(void) { return _value; }

public:
    bool isSafeInt(void) { return _value.isSafeInt(); }
    bool isSafeUInt(void) { return _value.isSafeUInt(); }
    bool isSafeNegativeUInt(void) { return _value.isSafeNegativeUInt(); }

public:
    int64_t toInt(void) { return _value.toInt(); }
    uint64_t toUInt(void) { return _value.toUInt(); }
    uint64_t toNegativeUInt(void) { return _value.toNegativeUInt(); }

public:
    static ObjectRef fromInt(int64_t value);
    static ObjectRef fromUInt(uint64_t value);
    static ObjectRef fromInteger(Utils::Integer &&value);
    static ObjectRef fromInteger(const Utils::Integer &value);

public:
    static void shutdown(void);
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_INTOBJECT_H */
