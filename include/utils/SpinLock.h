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
    Engine::Thread *_owner;
    std::atomic_flag _flags;

public:
    SpinLock() : _owner(nullptr) {}

public:
    Engine::Thread *owner(void) const { return _owner; }

public:
    class Scope
    {
        SpinLock &_lock;

    public:
       ~Scope()
       {
            _lock._owner = nullptr;
            _lock._flags.clear(std::memory_order_release);
       }

    public:
        Scope(SpinLock &lock) : _lock(lock)
        {
            while (_lock._flags.test_and_set(std::memory_order_acquire));
            _lock._owner = Engine::Thread::current();
        }
    };
};
}

#endif /* REDSCRIPT_UTILS_SPINLOCK_H */
