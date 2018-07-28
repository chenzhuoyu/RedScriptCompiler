#ifndef REDSCRIPT_EXCEPTIONS_TYPEERROR_H
#define REDSCRIPT_EXCEPTIONS_TYPEERROR_H

#include <string>

#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
class TypeError : public BaseException
{
public:
    TypeError(const std::string &message) :
        BaseException("TypeError", message) {}

};
}

#endif /* REDSCRIPT_EXCEPTIONS_TYPEERROR_H */
