#ifndef REDSCRIPT_EXCEPTIONS_ATTRIBUTEERROR_H
#define REDSCRIPT_EXCEPTIONS_ATTRIBUTEERROR_H

#include <string>

#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
class AttributeError : public BaseException
{
public:
    AttributeError(const std::string &message) :
        BaseException("AttributeError", message) {}

};
}

#endif /* REDSCRIPT_EXCEPTIONS_ATTRIBUTEERROR_H */
