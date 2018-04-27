#ifndef REDSCRIPT_RUNTIME_STRINGOBJECT_H
#define REDSCRIPT_RUNTIME_STRINGOBJECT_H

#include <vector>
#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class StringType : public Type
{
public:
    explicit StringType() : Type("str") {}

public:
    virtual uint64_t    objectHash(ObjectRef self) override;
    virtual std::string objectStr (ObjectRef self) override;
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

/* type object for string */
extern TypeRef StringTypeObject;

class StringObject : public Object
{
    std::string _value;
    friend class StringType;

public:
    virtual ~StringObject() = default;
    explicit StringObject(const std::string &value) : Object(StringTypeObject), _value(value) {}

public:
    const std::string &value(void) const { return _value; }

public:
    static ObjectRef fromString(const std::string &value);

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_STRINGOBJECT_H */
