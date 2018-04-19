#include "runtime/CodeObject.h"

namespace RedScript::Runtime
{
/* type object for code */
TypeRef CodeTypeObject;

void CodeObject::initialize(void)
{
    static CodeType type;
    CodeTypeObject = Reference<CodeType>::refStatic(type);
}
}
