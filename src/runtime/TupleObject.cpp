#include "runtime/TupleObject.h"

namespace RedScript::Runtime
{
/* type object for arrays */
TypeRef TupleTypeObject;

void TupleObject::initialize(void)
{
    /* tuple type object */
    static TupleType tupleType;
    TupleTypeObject = Reference<TupleType>::refStatic(tupleType);
}
}
