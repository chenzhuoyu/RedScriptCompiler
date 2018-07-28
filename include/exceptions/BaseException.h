#ifndef REDSCRIPT_EXCEPTIONS_BASEEXCEPTION_H
#define REDSCRIPT_EXCEPTIONS_BASEEXCEPTION_H

#include <string>
#include <vector>
#include <exception>

#include "utils/Strings.h"
#include "utils/Preprocessor.h"

namespace RedScript::Exceptions
{
struct BaseException : public std::exception
{
    struct Traceback
    {
        int row;
        int col;
        std::string file;
        std::string name;
    };

private:
    std::string _name;
    std::string _message;
    std::vector<Traceback> _traceback;

public:
    explicit BaseException(const std::string &message) : BaseException("BaseException", message) {}
    explicit BaseException(const std::string &name, const std::string &message);

public:
    const auto &name(void) const { return _name; }
    const auto &message(void) const { return _message; }
    const auto &traceback(void) const { return _traceback; }

public:
    virtual const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("%s: %s", _name, _message)).c_str();
    }
};
}

#endif /* REDSCRIPT_EXCEPTIONS_BASEEXCEPTION_H */
