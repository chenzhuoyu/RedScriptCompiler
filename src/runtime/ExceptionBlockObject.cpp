#include "runtime/ExceptionBlockObject.h"

namespace RedScript::Runtime
{
/* type object for exception block */
TypeRef ExceptionBlockTypeObject;

void ExceptionBlockObject::initialize(void)
{
    /* exception block type object */
    static ExceptionBlockType exceptionBlockType;
    ExceptionBlockTypeObject = Reference<ExceptionBlockType>::refStatic(exceptionBlockType);
}
}
