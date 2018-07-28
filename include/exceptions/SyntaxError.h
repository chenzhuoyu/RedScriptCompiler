#ifndef REDSCRIPT_EXCEPTIONS_SYNTAXERROR_H
#define REDSCRIPT_EXCEPTIONS_SYNTAXERROR_H

#include <string>

#include "utils/Strings.h"
#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
class SyntaxError : public BaseException
{
    int _row;
    int _col;

public:
    SyntaxError(int row, int col, const std::string &message) :
        BaseException("SyntaxError", message), _row(row), _col(col) {}

public:
    template <typename T> explicit SyntaxError(T &&t) : SyntaxError(t, "Unexpected token " + t->toString()) {}
    template <typename T> explicit SyntaxError(T &&t, const std::string &message) : SyntaxError(t->row(), t->col(), message) {}

public:
    int row(void) const { return _row; }
    int col(void) const { return _col; }

public:
    virtual const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("SyntaxError: %s at %d:%d", message(), _row, _col)).c_str();
    }
};
}

#endif /* REDSCRIPT_EXCEPTIONS_SYNTAXERROR_H */
