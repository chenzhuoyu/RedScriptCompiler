#ifndef REDSCRIPT_COMPILER_DECIMALOBJECT_H
#define REDSCRIPT_COMPILER_DECIMALOBJECT_H

#include <vector>
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
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_COMPILER_DECIMALOBJECT_H */
