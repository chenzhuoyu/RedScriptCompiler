#ifndef REDSCRIPT_RUNTIME_SLICEOBJECT_H
#define REDSCRIPT_RUNTIME_SLICEOBJECT_H

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class SliceType : public Type
{
public:
    explicit SliceType() : Type("slice") {}

};

/* type object for slice */
extern TypeRef SliceTypeObject;

class SliceObject : public Object
{
    ObjectRef _end;
    ObjectRef _step;
    ObjectRef _begin;

public:
    virtual ~SliceObject() = default;
    explicit SliceObject(ObjectRef begin, ObjectRef end, ObjectRef step) :
        Object(SliceTypeObject),
        _end(end),
        _step(step),
        _begin(begin) {}

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_SLICEOBJECT_H */