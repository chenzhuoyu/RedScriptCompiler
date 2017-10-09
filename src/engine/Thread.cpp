#include <atomic>
#include "engine/Thread.h"

namespace RedScript::Engine
{
/* thread object instance per real thread */
static thread_local Thread _thread;
static std::atomic_uint64_t _threadKey = 0;

Thread::Thread()
{
    /* acquire new thread key by atomic add */
    _key = _threadKey++;
}

Thread *Thread::current(void)
{
    /* current thread */
    return &_thread;
}
}
