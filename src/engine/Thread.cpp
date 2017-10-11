#include <atomic>
#include <stdexcept>

#include "engine/Thread.h"
#include "lockfree/IndexGenerator.h"

namespace RedScript::Engine
{
/* lock-free ID generator to generate thread keys */
static LockFree::IndexGenerator _threadKey(Thread::MAX_THREADS);

Thread::Thread()
{
    /* generate a unique ID for this thread */
    if ((_key = _threadKey.acquire()) < 0)
        throw std::overflow_error("Too much threads");
}

Thread::~Thread()
{
    /* release the thread key */
    _threadKey.release(_key);
}

size_t Thread::count(void)
{
    /* load the counter */
    return _threadKey.count();
}

Thread &Thread::current(void)
{
    static thread_local Thread thread;
    return thread;
}
}
