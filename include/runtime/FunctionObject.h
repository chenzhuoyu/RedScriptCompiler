#ifndef REDSCRIPT_RUNTIME_FUNCTIONOBJECT_H
#define REDSCRIPT_RUNTIME_FUNCTIONOBJECT_H

#include "runtime/Object.h"
#include "runtime/CodeObject.h"
#include "runtime/TupleObject.h"

namespace RedScript::Runtime
{
class FunctionType : public Type
{
public:
    explicit FunctionType() : Type("function") {}

};

/* type object for function */
extern TypeRef FunctionTypeObject;

class FunctionObject : public Object
{
    Reference<CodeObject> _code;
    Reference<TupleObject> _defaults;

public:
    virtual ~FunctionObject() = default;
    explicit FunctionObject(ObjectRef code, ObjectRef defaults);

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_FUNCTIONOBJECT_H */
