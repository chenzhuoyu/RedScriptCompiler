#ifndef REDSCRIPT_EXCEPTIONS_RUNTIMEERROR_H
#define REDSCRIPT_EXCEPTIONS_RUNTIMEERROR_H

#include <string>

#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
class RuntimeError : public BaseException
{
public:
    RuntimeError(const std::string &message) :
        BaseException("RuntimeError", message) {}

};
}

#endif /* REDSCRIPT_EXCEPTIONS_RUNTIMEERROR_H */
