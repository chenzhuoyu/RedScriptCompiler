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
    /* nothing */
};

/* type object for integer */
extern TypeRef IntTypeObject;

class IntObject : public Object
{
public:
    virtual ~IntObject() = default;
    explicit IntObject(int64_t value) : Object(IntTypeObject) {}

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_COMPILER_INTOBJECT_H */
