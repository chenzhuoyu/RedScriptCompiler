#ifndef COMPILATIONERROR_H
#define COMPILATIONERROR_H

#include <string>
#include <exception>

class CompilationError : public std::exception
{
    int _row;
    int _col;
    std::string _message;

public:
    explicit CompilationError(int row, int col, const std::string &message) : _row(row), _col(col), _message(message) {}

public:
    int row(void) const { return _row; }
    int col(void) const { return _col; }
    const std::string &message(void) const { return _message; }

public:
    const char *what() const noexcept override { return _message.c_str(); }

};

#endif /* COMPILATIONERROR_H */
