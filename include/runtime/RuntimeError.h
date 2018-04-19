#ifndef REDSCRIPT_COMPILER_RUNTIMEERROR_H
#define REDSCRIPT_COMPILER_RUNTIMEERROR_H

#include <string>
#include <exception>

#include "utils/Strings.h"

namespace RedScript::Runtime
{
class RuntimeError : public std::exception
{
    int _code;
    std::string _message;

public:
    explicit RuntimeError(const std::string &message) : RuntimeError(-1, message) {}
    explicit RuntimeError(int code, const std::string &message) : _code(code), _message(message) {}

public:
    int code(void) const { return _code; }
    const std::string &message(void) const { return _message; }

public:
    const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("RuntimeError: [%d] %s", _code, _message)).c_str();
    }
};
}

#endif /* REDSCRIPT_COMPILER_RUNTIMEERROR_H */
