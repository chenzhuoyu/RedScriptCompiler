#ifndef REDSCRIPT_RUNTIME_BOOLOBJECT_H
#define REDSCRIPT_RUNTIME_BOOLOBJECT_H

#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class BoolType : public Type
{
public:
    explicit BoolType() : Type("bool") {}

public:
    virtual uint64_t    objectHash(ObjectRef self) override;
    virtual std::string objectRepr(ObjectRef self) override;

public:
    virtual bool objectIsTrue(ObjectRef self) override;

};

/* type object for boolean */
extern TypeRef BoolTypeObject;

class BoolObject : public Object
{
    bool _value;
    friend class BoolType;

public:
    virtual ~BoolObject() = default;
    explicit BoolObject(bool value) : Object(BoolTypeObject), _value(value) {}

public:
    static ObjectRef fromBool(bool value);

public:
    static void shutdown(void) {}
    static void initialize(void);

};

/* two static boolean constants */
extern ObjectRef TrueObject;
extern ObjectRef FalseObject;
}

#endif /* REDSCRIPT_RUNTIME_BOOLOBJECT_H */
