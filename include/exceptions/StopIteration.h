#ifndef REDSCRIPT_EXCEPTIONS_STOPITERATION_H
#define REDSCRIPT_EXCEPTIONS_STOPITERATION_H

#include "exceptions/BaseException.h"

namespace RedScript::Exceptions
{
class StopIteration : public BaseException
{
public:
    StopIteration() : BaseException("StopIteration", "Iterator drained") {}
    virtual const char *what() const noexcept override { return "StopIteration"; }

};
}

#endif /* REDSCRIPT_EXCEPTIONS_STOPITERATION_H */
