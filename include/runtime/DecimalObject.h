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

private:
    static bool compare(ObjectRef self, ObjectRef other, bool (*ret)(int));
    static Utils::Decimal toDecimal(ObjectRef other);

/*** Object Protocol ***/

public:
    virtual uint64_t    objectHash(ObjectRef self) override;
    virtual std::string objectRepr(ObjectRef self) override;

public:
    virtual bool objectIsTrue(ObjectRef self) override;

/*** Numeric Protocol ***/

public:
    virtual ObjectRef numericPos(ObjectRef self) override;
    virtual ObjectRef numericNeg(ObjectRef self) override;

public:
    virtual ObjectRef numericAdd  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericSub  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericMul  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericDiv  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericMod  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericPower(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef numericIncAdd  (ObjectRef self, ObjectRef other) override { return numericAdd  (self, other); }
    virtual ObjectRef numericIncSub  (ObjectRef self, ObjectRef other) override { return numericSub  (self, other); }
    virtual ObjectRef numericIncMul  (ObjectRef self, ObjectRef other) override { return numericMul  (self, other); }
    virtual ObjectRef numericIncDiv  (ObjectRef self, ObjectRef other) override { return numericDiv  (self, other); }
    virtual ObjectRef numericIncMod  (ObjectRef self, ObjectRef other) override { return numericMod  (self, other); }
    virtual ObjectRef numericIncPower(ObjectRef self, ObjectRef other) override { return numericPower(self, other); }

/*** Comparator Protocol ***/

public:
    virtual ObjectRef comparableEq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableLt(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableGt(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableNeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableLeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableGeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableCompare(ObjectRef self, ObjectRef other) override;

};

/* type object for decimal */
extern TypeRef DecimalTypeObject;

class DecimalObject : public Object
{
    Utils::Decimal _value;
    friend class DecimalType;

public:
    virtual ~DecimalObject() = default;
    explicit DecimalObject(Utils::Decimal value) : Object(DecimalTypeObject), _value(value) {}

public:
    bool isSafeFloat(void) { return _value.isSafeFloat(); }
    bool isSafeDouble(void) { return _value.isSafeDouble(); }

public:
    auto value(void) { return _value; }
    float toFloat(void) { return _value.toFloat(); }
    double toDouble(void) { return _value.toDouble(); }

public:
    static ObjectRef fromDouble(double value) { return Object::newObject<DecimalObject>(value); }
    static ObjectRef fromDecimal(Utils::Decimal value) { return Object::newObject<DecimalObject>(value); }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_DECIMALOBJECT_H */
