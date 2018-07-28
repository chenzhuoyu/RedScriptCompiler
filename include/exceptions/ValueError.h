#ifndef REDSCRIPT_EXCEPTIONS_VALUEERROR_H
#define REDSCRIPT_EXCEPTIONS_VALUEERROR_H

#include <string>

#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
class ValueError : public BaseException
{
public:
    ValueError(const std::string &message) :
        BaseException("ValueError", message) {}

};
}

#endif /* REDSCRIPT_EXCEPTIONS_VALUEERROR_H */
