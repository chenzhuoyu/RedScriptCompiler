#ifndef REDSCRIPT_RUNTIME_DECIMALOBJECT_H
#define REDSCRIPT_RUNTIME_DECIMALOBJECT_H

#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class DecimalType : public Type
{
public:
    explicit DecimalType() : Type("decimal") {}

public:
    virtual bool objectIsTrue(ObjectRef self) override;

};

/* type object for decimal */
extern TypeRef DecimalTypeObject;

class DecimalObject : public Object
{
    // TODO use full precision arithmetic
    friend class DecimalType;

public:
    virtual ~DecimalObject() = default;
    explicit DecimalObject(double value) : Object(DecimalTypeObject) {}

public:
    static ObjectRef fromDouble(double value);
    static ObjectRef fromString(const std::string &value);


public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_DECIMALOBJECT_H */
