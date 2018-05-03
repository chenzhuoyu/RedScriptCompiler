#ifndef REDSCRIPT_RUNTIME_TUPLEOBJECT_H
#define REDSCRIPT_RUNTIME_TUPLEOBJECT_H

#include <string>
#include <cstdint>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class TupleType : public Type
{
public:
    explicit TupleType() : Type("tuple") {}

};

/* type object for tuples */
extern TypeRef TupleTypeObject;

class TupleObject : public Object
{
    size_t _size;
    ObjectRef *_items;

public:
    virtual ~TupleObject() { delete [] _items; }
    explicit TupleObject(size_t size) : Object(TupleTypeObject), _items(new ObjectRef[size]) {}

public:
    size_t size(void) const { return _size; }
    ObjectRef *items(void) const { return _items; }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_TUPLEOBJECT_H */
