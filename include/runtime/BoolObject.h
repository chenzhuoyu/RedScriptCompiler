#ifndef REDSCRIPT_RUNTIME_BOOLOBJECT_H
#define REDSCRIPT_RUNTIME_BOOLOBJECT_H

#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class BoolType : public NativeType
{
public:
    explicit BoolType() : NativeType("bool") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual uint64_t    nativeObjectHash(ObjectRef self) override;
    virtual std::string nativeObjectRepr(ObjectRef self) override;

public:
    virtual bool nativeObjectIsTrue(ObjectRef self) override;

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
    bool value(void) { return _value; }

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
