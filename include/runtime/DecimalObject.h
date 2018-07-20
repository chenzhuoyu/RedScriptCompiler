#ifndef REDSCRIPT_RUNTIME_DECIMALOBJECT_H
#define REDSCRIPT_RUNTIME_DECIMALOBJECT_H

#include <string>
#include <cstdint>

#include "utils/Decimal.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class DecimalType : public Type
{
public:
    explicit DecimalType() : Type("decimal") {}

protected:
    virtual void addBuiltins(void) override;
    virtual void clearBuiltins(void) override {}

private:
    static bool compare(ObjectRef self, ObjectRef other, bool (*ret)(int));
    static Utils::Decimal toDecimal(ObjectRef other);

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

public:
    virtual ObjectRef nativeNumericAdd  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericSub  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericMul  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericDiv  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericMod  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericPower(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef nativeNumericIncAdd  (ObjectRef self, ObjectRef other) override { return nativeNumericAdd  (self, other); }
    virtual ObjectRef nativeNumericIncSub  (ObjectRef self, ObjectRef other) override { return nativeNumericSub  (self, other); }
    virtual ObjectRef nativeNumericIncMul  (ObjectRef self, ObjectRef other) override { return nativeNumericMul  (self, other); }
    virtual ObjectRef nativeNumericIncDiv  (ObjectRef self, ObjectRef other) override { return nativeNumericDiv  (self, other); }
    virtual ObjectRef nativeNumericIncMod  (ObjectRef self, ObjectRef other) override { return nativeNumericMod  (self, other); }
    virtual ObjectRef nativeNumericIncPower(ObjectRef self, ObjectRef other) override { return nativeNumericPower(self, other); }

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

/* type object for decimal */
extern TypeRef DecimalTypeObject;

class DecimalObject : public Object
{
    Utils::Decimal _value;
    friend class DecimalType;

public:
    virtual ~DecimalObject() = default;
    explicit DecimalObject(const Utils::Decimal &value) : Object(DecimalTypeObject), _value(value) {}

public:
    const Utils::Decimal &value(void) { return _value; }

public:
    bool isSafeFloat(void) { return _value.isSafeFloat(); }
    bool isSafeDouble(void) { return _value.isSafeDouble(); }

public:
    float toFloat(void) { return _value.toFloat(); }
    double toDouble(void) { return _value.toDouble(); }

public:
    static ObjectRef fromDouble(double value) { return Object::newObject<DecimalObject>(value); }
    static ObjectRef fromDecimal(const Utils::Decimal &value) { return Object::newObject<DecimalObject>(value); }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_DECIMALOBJECT_H */
