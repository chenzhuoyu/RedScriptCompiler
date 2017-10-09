#ifndef REDSCRIPT_ENGINE_THREAD_H
#define REDSCRIPT_ENGINE_THREAD_H

#include <cstdint>

namespace RedScript::Engine
{
class Thread final
{
    uint64_t _key;

public:
    Thread();

public:
    uint64_t key(void) const { return _key; }

public:
    static Thread *current(void);

};
}

#endif /* REDSCRIPT_ENGINE_THREAD_H */
