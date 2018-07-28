#ifndef REDSCRIPT_EXCEPTIONS_USERDEFINEDERROR_H
#define REDSCRIPT_EXCEPTIONS_USERDEFINEDERROR_H

#include "runtime/Object.h"
#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
struct UserDefinedError : public BaseException
{
    mutable Runtime::ObjectRef self;
    explicit UserDefinedError(Runtime::ObjectRef &&self) : BaseException("UserDefinedError"), self(std::move(self)) {}

public:
    virtual const char *what() const noexcept override
    {
        static thread_local std::string what;
        return (what = Utils::Strings::format("%s: %s", self->type()->name(), self->type()->objectStr(self))).c_str();
    }
};

}

#endif /* REDSCRIPTCOMPILER_USERDEFINEDERROR_H */
