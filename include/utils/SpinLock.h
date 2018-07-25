#ifndef REDSCRIPT_UTILS_SPINLOCK_H
#define REDSCRIPT_UTILS_SPINLOCK_H

#include <atomic>
#include <shared_mutex>

#include "engine/Thread.h"
#include "utils/Immovable.h"
#include "utils/NonCopyable.h"

namespace RedScript::Utils
{
class SpinLock final : public Immovable, public NonCopyable
{
    int _flags;
    Engine::Thread *_owner;

public:
    SpinLock() : _flags(0), _owner(nullptr) {}

public:
    bool isLocked(void) const { return _flags != 0; }
    Engine::Thread *owner(void) const { return _owner; }

public:
    class Scope
    {
        SpinLock &_lock;

    public:
       ~Scope()
       {
            _lock._owner = nullptr;
            __sync_lock_release(&(_lock._flags));
       }

    public:
        Scope(SpinLock &lock) : _lock(lock)
        {
            while (__sync_lock_test_and_set(&(_lock._flags), 1));
            _lock._owner = Engine::Thread::current();
        }
    };
};
}

#endif /* REDSCRIPT_UTILS_SPINLOCK_H */
