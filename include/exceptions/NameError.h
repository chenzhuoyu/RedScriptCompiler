#ifndef REDSCRIPT_EXCEPTIONS_NAMEERROR_H
#define REDSCRIPT_EXCEPTIONS_NAMEERROR_H

#include <string>
#include <exception>

#include "utils/Strings.h"

namespace RedScript::Exceptions
{
class NameError : public std::exception
{
    int _row;
    int _col;
    std::string _message;

public:
    explicit NameError(int row, int col, const std::string &message) : _row(row), _col(col), _message(message) {}

public:
    int row(void) const { return _row; }
    int col(void) const { return _col; }
    const std::string &message(void) const { return _message; }

public:
    virtual const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("NameError: %s at %d:%d", _message, _row, _col)).c_str();
    }
};
}

#endif /* REDSCRIPT_EXCEPTIONS_NAMEERROR_H */
