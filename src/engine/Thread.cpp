#include <atomic>
#include <stdexcept>

#include "engine/Thread.h"
#include "engine/Memory.h"
#include "lockfree/IndexGenerator.h"

namespace RedScript::Engine
{
struct ThreadControlBlock
{
    std::atomic_size_t       reclaim;
    LockFree::IndexGenerator localKey;
    LockFree::IndexGenerator threadKey;

public:
    ThreadControlBlock() : reclaim(0), localKey(Thread::MAX_LOCALS), threadKey(Thread::MAX_THREADS) {}

};

/* thread control blocks */
static ThreadControlBlock *_tcb = new ThreadControlBlock;

Thread::Thread() : _locals(MIN_LOCALS)
{
    /* generate a unique ID for this thread */
    if ((_key = _tcb->threadKey.acquire()) < 0)
        throw std::overflow_error("Too much threads");
}

Thread::~Thread()
{
    /* clear all thread locals */
    for (Storage *local : _locals)
    {
        if (local != nullptr)
        {
            local->destroy(local->ptr);
            delete local;
        }
    }

    /* release the thread key */
    _locals.clear();
    _tcb->threadKey.release(_key);

    /* release the tcb if no threads alive */
    if (!_tcb->threadKey.count())
    {
        delete _tcb;
        _tcb = nullptr;
    }
}

size_t Thread::count(void)
{
    /* load the counter */
    return _tcb->threadKey.count();
}

Thread &Thread::current(void)
{
    static thread_local Thread thread;
    return thread;
}

int Thread::allocLocalId(size_t &reclaim)
{
    /* acquire from index generator */
    reclaim = _tcb->reclaim++;
    return _tcb->localKey.acquire();
}

void Thread::releaseLocalId(int id)
{
    /* release back into index generator */
    _tcb->localKey.release(id);
}
}
