#ifndef REDSCRIPT_EXCEPTIONS_INDEXERROR_H
#define REDSCRIPT_EXCEPTIONS_INDEXERROR_H

#include <string>
#include <exception>

#include "utils/Strings.h"

namespace RedScript::Exceptions
{
class IndexError : public std::exception
{
    int _code;
    std::string _message;

public:
    explicit IndexError(const std::string &message) : IndexError(-1, message) {}
    explicit IndexError(int code, const std::string &message) : _code(code), _message(message) {}

public:
    int code(void) const { return _code; }
    const std::string &message(void) const { return _message; }

public:
    const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("IndexError: [%d] %s", _code, _message)).c_str();
    }
};
}

#endif /* REDSCRIPT_EXCEPTIONS_INDEXERROR_H */
