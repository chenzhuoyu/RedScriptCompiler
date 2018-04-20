#ifndef REDSCRIPT_COMPILER_BOOLOBJECT_H
#define REDSCRIPT_COMPILER_BOOLOBJECT_H

#include <vector>
#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class BoolType : public Type
{
public:
    using Type::Type;

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

#endif /* REDSCRIPT_COMPILER_BOOLOBJECT_H */
