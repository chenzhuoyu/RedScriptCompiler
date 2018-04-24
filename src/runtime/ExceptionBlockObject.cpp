#include "utils/Strings.h"
#include "runtime/ExceptionBlockObject.h"

namespace RedScript::Runtime
{
/* type object for exception block */
TypeRef ExceptionBlockTypeObject;

std::string ExceptionBlockType::objectRepr(ObjectRef self)
{
    auto block = self.as<ExceptionBlockObject>();
    return Utils::Strings::format(
        "<block except=%s finally=%s>",
        block->_hasExcept ? Utils::Strings::format("%#x", block->_except) : "(null)",
        block->_hasFinally ? Utils::Strings::format("%#x", block->_finally) : "(null)"
    );
}

void ExceptionBlockObject::initialize(void)
{
    /* exception block type object */
    static ExceptionBlockType exceptionBlockType;
    ExceptionBlockTypeObject = Reference<ExceptionBlockType>::refStatic(exceptionBlockType);
}
}
