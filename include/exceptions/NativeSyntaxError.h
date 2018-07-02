#ifndef REDSCRIPT_EXCEPTIONS_NATIVESYNTAXERROR_H
#define REDSCRIPT_EXCEPTIONS_NATIVESYNTAXERROR_H

#include "SyntaxError.h"

namespace RedScript::Exceptions
{
class NativeSyntaxError : public SyntaxError
{
    bool _isWarning;
    std::string _filename;

public:
    explicit NativeSyntaxError(const std::string &filename, int row, bool isWarning, const std::string &message) :
        SyntaxError(row, -1, message), _isWarning(isWarning), _filename(filename) {}

public:
    bool isWarning(void) const { return _isWarning; }
    const std::string &filename(void) const { return _filename; }

public:
    virtual const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("(%s:%d) %s :: %s", _filename, row(), _isWarning ? "WARNING" : "ERROR", message())).c_str();
    }
};
}

#endif /* REDSCRIPT_EXCEPTIONS_NATIVESYNTAXERROR_H */