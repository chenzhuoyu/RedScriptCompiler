#ifndef REDSCRIPT_ENGINE_THREAD_H
#define REDSCRIPT_ENGINE_THREAD_H

#include <cstdint>

namespace RedScript::Engine
{
struct Thread final
{
    static constexpr size_t MAX_THREADS = 65536;

private:
    int _key;

private:
    Thread();
   ~Thread();

public:
    int key(void) const { return _key; }

public:
    static size_t count(void);
    static Thread &current(void);

};
}

#endif /* REDSCRIPT_ENGINE_THREAD_H */
