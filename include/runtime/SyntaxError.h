#ifndef REDSCRIPT_RUNTIME_SYNTAXERROR_H
#define REDSCRIPT_RUNTIME_SYNTAXERROR_H

#include <string>
#include <exception>

#include "utils/Strings.h"

namespace RedScript::Runtime
{
class SyntaxError : public std::exception
{
    int _row;
    int _col;
    std::string _message;

public:
    template <typename T>
    explicit SyntaxError(T &&t) : SyntaxError(t, "Unexpected token " + t->toString()) {}

public:
    template <typename T>
    explicit SyntaxError(T &&t, const std::string &message) : SyntaxError(t->row(), t->col(), message) {}

public:
    explicit SyntaxError(int row, int col, const std::string &message) : _row(row), _col(col), _message(message) {}

public:
    int row(void) const { return _row; }
    int col(void) const { return _col; }
    const std::string &message(void) const { return _message; }

public:
    const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("%s at %d:%d", _message, _row, _col)).c_str();
    }
};
}

#endif /* REDSCRIPT_RUNTIME_SYNTAXERROR_H */
