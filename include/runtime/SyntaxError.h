#ifndef REDSCRIPT_RUNTIME_COMPILATIONERROR_H
#define REDSCRIPT_RUNTIME_COMPILATIONERROR_H

#include <string>
#include <exception>

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
    const char *what() const noexcept override { return _message.c_str(); }

};
}

#endif /* REDSCRIPT_RUNTIME_COMPILATIONERROR_H */
