#ifndef REDSCRIPT_ENGINE_THREAD_H
#define REDSCRIPT_ENGINE_THREAD_H

namespace RedScript::Engine
{
class Thread
{
public:
    static Thread *current(void)
    {
        static thread_local Thread thread;
        return &thread;
    }
};
}

#endif /* REDSCRIPT_ENGINE_THREAD_H */
