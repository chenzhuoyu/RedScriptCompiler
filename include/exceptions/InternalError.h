#ifndef REDSCRIPT_EXCEPTIONS_INTERNALERROR_H
#define REDSCRIPT_EXCEPTIONS_INTERNALERROR_H

#include <string>

#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
class InternalError : public BaseException
{
public:
    InternalError(const std::string &message) :
        BaseException("InternalError", message) {}

};
}

#endif /* REDSCRIPT_EXCEPTIONS_INTERNALERROR_H */
