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
    using Type::Type;

public:
    virtual bool objectIsTrue(ObjectRef self) override;

};

/* type object for integer */
extern TypeRef IntTypeObject;

class IntObject : public Object
{
    // TODO use high precision arithmetic
    friend class IntType;

public:
    virtual ~IntObject() = default;
    explicit IntObject(int64_t value) : Object(IntTypeObject) {}

public:
    static ObjectRef fromInt(int64_t value);
    static ObjectRef fromString(const std::string &value);

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_COMPILER_INTOBJECT_H */
