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
    int64_t toInt(void) { return _value.toInt(); }
    uint64_t toUInt(void) { return _value.toUInt(); }

public:
    static ObjectRef fromInt(int64_t value);
    static ObjectRef fromUInt(uint64_t value);
    static ObjectRef fromInteger(Utils::Integer value) { return Object::newObject<IntObject>(value); }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_INTOBJECT_H */
