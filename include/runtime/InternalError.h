#ifndef REDSCRIPT_RUNTIME_INTERNALERROR_H
#define REDSCRIPT_RUNTIME_INTERNALERROR_H

#include <string>
#include <exception>

#include "utils/Strings.h"

namespace RedScript::Runtime
{
class InternalError : public std::exception
{
    int _code;
    std::string _message;

public:
    explicit InternalError(const std::string &message) : InternalError(-1, message) {}
    explicit InternalError(int code, const std::string &message) : _code(code), _message(message) {}

public:
    int code(void) const { return _code; }
    const std::string &message(void) const { return _message; }

public:
    const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("InternalError: [%d] %s", _code, _message)).c_str();
    }
};
}

#endif /* REDSCRIPT_RUNTIME_INTERNALERROR_H */
