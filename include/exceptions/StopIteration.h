#ifndef REDSCRIPT_EXCEPTIONS_STOPITERATION_H
#define REDSCRIPT_EXCEPTIONS_STOPITERATION_H

#include <exception>

namespace RedScript::Exceptions
{
class StopIteration : public std::exception
{
public:
    virtual const char *what() const noexcept override
    {
        /* theoratically, this is not an exception */
        return "StopIteration";
    }
};
}

#endif /* REDSCRIPT_EXCEPTIONS_STOPITERATION_H */
