#include "runtime/StringObject.h"

namespace RedScript::Runtime
{
/* type object for string */
TypeRef StringTypeObject;

void StringObject::initialize(void)
{
    static StringType type;
    StringTypeObject = Reference<StringType>::refStatic(type);
}
}
