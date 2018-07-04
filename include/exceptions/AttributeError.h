#ifndef REDSCRIPT_EXCEPTIONS_ATTRIBUTEERROR_H
#define REDSCRIPT_EXCEPTIONS_ATTRIBUTEERROR_H

#include <string>
#include <exception>

#include "utils/Strings.h"

namespace RedScript::Exceptions
{
class AttributeError : public std::exception
{
    int _code;
    std::string _message;

public:
    explicit AttributeError(const std::string &message) : AttributeError(-1, message) {}
    explicit AttributeError(int code, const std::string &message) : _code(code), _message(message) {}

public:
    int code(void) const { return _code; }
    const std::string &message(void) const { return _message; }

public:
    const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("AttributeError: [%d] %s", _code, _message)).c_str();
    }
};
}

#endif /* REDSCRIPT_EXCEPTIONS_ATTRIBUTEERROR_H */
