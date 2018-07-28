#ifndef REDSCRIPT_EXCEPTIONS_INDEXERROR_H
#define REDSCRIPT_EXCEPTIONS_INDEXERROR_H

#include <string>

#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
class IndexError : public BaseException
{
public:
    IndexError(const std::string &message) :
        BaseException("IndexError", message) {}

};
}

#endif /* REDSCRIPT_EXCEPTIONS_INDEXERROR_H */
