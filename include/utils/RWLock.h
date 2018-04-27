#ifndef REDSCRIPT_UTILS_READWRITELOCK_H
#define REDSCRIPT_UTILS_READWRITELOCK_H

#include <atomic>
#include <shared_mutex>

#include "engine/Thread.h"
#include "utils/Immovable.h"
#include "utils/NonCopyable.h"

namespace RedScript::Utils
{
class RWLock final : public Immovable, public NonCopyable
{
    Engine::Thread *_owner;
    std::shared_mutex _mutex;
    std::atomic_size_t _reads;

private:
    friend class LockRead;
    friend class LockWrite;

public:
    RWLock() : _owner(nullptr), _reads(0) {}

public:
    size_t reads(void) const { return _reads.load(); }
    Engine::Thread *owner(void) const { return _owner; }

public:
    class Read
    {
        RWLock &_lock;

    public:
        ~Read()
        {
            _lock._reads--;
            _lock._mutex.unlock_shared();
        }

    public:
        Read(RWLock &lock) : _lock(lock)
        {
            _lock._mutex.lock_shared();
            _lock._reads++;
        }
    };

public:
    class Write
    {
        RWLock &_lock;

    public:
        ~Write()
        {
            _lock._owner = nullptr;
            _lock._mutex.unlock();
        }

    public:
        Write(RWLock &lock) : _lock(lock)
        {
            _lock._mutex.lock();
            _lock._owner = Engine::Thread::current();
        }
    };
};
}

#endif /* REDSCRIPT_UTILS_READWRITELOCK_H */
