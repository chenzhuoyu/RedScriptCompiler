#ifndef REDSCRIPT_COMPILER_INTOBJECT_H
#define REDSCRIPT_COMPILER_INTOBJECT_H

#include <vector>
#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class IntType : public Type
{
public:
    explicit IntType() : Type("int") {}

public:
    virtual uint64_t    objectHash(ObjectRef self) override;
    virtual std::string objectStr (ObjectRef self) override;
    virtual std::string objectRepr(ObjectRef self) override;

public:
    virtual bool objectIsTrue(ObjectRef self) override;

public:
    virtual ObjectRef comparableEq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableLe(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableGe(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableNeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableLeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableGeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableCompare(ObjectRef self, ObjectRef other) override;

};

/* type object for integer */
extern TypeRef IntTypeObject;

class IntObject : public Object
{
    // TODO use high precision arithmetic
    int64_t _value;
    friend class IntType;

public:
    virtual ~IntObject() = default;
    explicit IntObject(int64_t value) : Object(IntTypeObject), _value(value) {}

public:
    static ObjectRef fromInt(int64_t value);
    static ObjectRef fromString(const std::string &value);

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_COMPILER_INTOBJECT_H */
