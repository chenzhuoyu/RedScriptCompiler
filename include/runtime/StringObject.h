#ifndef REDSCRIPT_COMPILER_STRINGOBJECT_H
#define REDSCRIPT_COMPILER_STRINGOBJECT_H

#include <vector>
#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class StringType : public Type
{
public:
    using Type::Type;

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
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_COMPILER_STRINGOBJECT_H */
