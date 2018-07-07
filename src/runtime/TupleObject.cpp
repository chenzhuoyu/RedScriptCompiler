#include "runtime/TupleObject.h"

namespace RedScript::Runtime
{
/* type object for arrays */
TypeRef TupleTypeObject;

TupleObject::TupleObject(size_t size) :
    Object(TupleTypeObject),
    _size(size),
    _items(new ObjectRef[size])
{
    if (size > 0)
        track();
}

void TupleObject::referenceClear(void)
{
    for (size_t i = 0; i < _size; i++)
        _items[i] = nullptr;
}

void TupleObject::referenceTraverse(VisitFunction visit)
{
    for (size_t i = 0; i < _size; i++)
        if (!(_items[i].isNull()))
            visit(_items[i]);
}

void TupleObject::initialize(void)
{
    /* tuple type object */
    static TupleType tupleType;
    TupleTypeObject = Reference<TupleType>::refStatic(tupleType);
}
}
