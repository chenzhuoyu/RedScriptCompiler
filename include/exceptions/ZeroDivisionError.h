#ifndef REDSCRIPT_EXCEPTIONS_ZERODIVISIONERROR_H
#define REDSCRIPT_EXCEPTIONS_ZERODIVISIONERROR_H

#include <string>

#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
class ZeroDivisionError : public BaseException
{
public:
    ZeroDivisionError(const std::string &message) :
        BaseException("ZeroDivisionError", message) {}

};
}

#endif /* REDSCRIPT_EXCEPTIONS_ZERODIVISIONERROR_H */
