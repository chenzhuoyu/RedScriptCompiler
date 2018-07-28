#ifndef REDSCRIPT_EXCEPTIONS_NAMEERROR_H
#define REDSCRIPT_EXCEPTIONS_NAMEERROR_H

#include <string>

#include "utils/Strings.h"
#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
class NameError : public BaseException
{
    int _row;
    int _col;

public:
    NameError(int row, int col, const std::string &message) :
        BaseException("NameError", message), _row(row), _col(col) {}

public:
    int row(void) const { return _row; }
    int col(void) const { return _col; }

public:
    virtual const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("NameError: %s at %d:%d", message(), _row, _col)).c_str();
    }
};
}

#endif /* REDSCRIPT_EXCEPTIONS_NAMEERROR_H */
