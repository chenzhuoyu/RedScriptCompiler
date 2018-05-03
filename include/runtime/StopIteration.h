#ifndef REDSCRIPT_RUNTIME_STOPITERATION_H
#define REDSCRIPT_RUNTIME_STOPITERATION_H

#include <exception>

namespace RedScript::Runtime
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

#endif /* REDSCRIPT_RUNTIME_STOPITERATION_H */
