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
    /* nothing */
};

/* type object for string */
extern TypeRef StringTypeObject;

class StringObject : public Object
{
public:
    virtual ~StringObject() = default;
    explicit StringObject(const std::string &value) : Object(StringTypeObject) {}

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_COMPILER_STRINGOBJECT_H */
