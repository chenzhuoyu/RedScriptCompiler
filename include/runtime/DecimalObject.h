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

public:
    virtual uint64_t    objectHash(ObjectRef self) override;
    virtual std::string objectRepr(ObjectRef self) override;

public:
    virtual bool objectIsTrue(ObjectRef self) override;

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
