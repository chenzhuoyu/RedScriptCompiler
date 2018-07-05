#ifndef REDSCRIPT_RUNTIME_INTOBJECT_H
#define REDSCRIPT_RUNTIME_INTOBJECT_H

#include <string>
#include <cstdint>
#include <utils/Decimal.h>

#include "utils/Integer.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class IntType : public Type
{
public:
    explicit IntType() : Type("int") {}

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
    virtual ObjectRef numericNot(ObjectRef self) override;

public:
    virtual ObjectRef numericAdd  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericSub  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericMul  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericDiv  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericMod  (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericPower(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef numericOr (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericAnd(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericXor(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef numericLShift(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericRShift(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef numericIncAdd  (ObjectRef self, ObjectRef other) override { return numericAdd  (self, other); }
    virtual ObjectRef numericIncSub  (ObjectRef self, ObjectRef other) override { return numericSub  (self, other); }
    virtual ObjectRef numericIncMul  (ObjectRef self, ObjectRef other) override { return numericMul  (self, other); }
    virtual ObjectRef numericIncDiv  (ObjectRef self, ObjectRef other) override { return numericDiv  (self, other); }
    virtual ObjectRef numericIncMod  (ObjectRef self, ObjectRef other) override { return numericMod  (self, other); }
    virtual ObjectRef numericIncPower(ObjectRef self, ObjectRef other) override { return numericPower(self, other); }

public:
    virtual ObjectRef numericIncOr (ObjectRef self, ObjectRef other) override { return numericOr (self, other); }
    virtual ObjectRef numericIncAnd(ObjectRef self, ObjectRef other) override { return numericAnd(self, other); }
    virtual ObjectRef numericIncXor(ObjectRef self, ObjectRef other) override { return numericXor(self, other); }

public:
    virtual ObjectRef numericIncLShift(ObjectRef self, ObjectRef other) override { return numericLShift(self, other); }
    virtual ObjectRef numericIncRShift(ObjectRef self, ObjectRef other) override { return numericRShift(self, other); }

/*** Comparable Protocol ***/

public:
    virtual ObjectRef comparableEq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableLt(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableGt(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableNeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableLeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableGeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableCompare(ObjectRef self, ObjectRef other) override;

};

/* type object for integer */
extern TypeRef IntTypeObject;

class IntObject : public Object
{
    Utils::Integer _value;
    friend class IntType;

public:
    virtual ~IntObject() = default;
    explicit IntObject(Utils::Integer value) : Object(IntTypeObject), _value(value) {}

public:
    bool isSafeInt(void) { return _value.isSafeInt(); }
    bool isSafeUInt(void) { return _value.isSafeUInt(); }

public:
    auto value(void) { return _value; }
    int64_t toInt(void) { return _value.toInt(); }
    uint64_t toUInt(void) { return _value.toUInt(); }

public:
    static ObjectRef fromInt(int64_t value);
    static ObjectRef fromUInt(uint64_t value);
    static ObjectRef fromInteger(Utils::Integer value) { return Object::newObject<IntObject>(value); }

public:
    static void shutdown(void);
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_INTOBJECT_H */
