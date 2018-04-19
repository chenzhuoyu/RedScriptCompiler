#ifndef REDSCRIPT_COMPILER_CODEOBJECT_H
#define REDSCRIPT_COMPILER_CODEOBJECT_H

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class CodeType : public Type
{
    /* nothing */
};

/* type object for code */
extern TypeRef CodeTypeObject;

class CodeObject : public Object
{
    friend class CodeType;

public:
    virtual ~CodeObject() = default;
    explicit CodeObject() : Object(CodeTypeObject) {}

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_COMPILER_CODEOBJECT_H */
