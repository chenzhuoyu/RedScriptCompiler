#ifndef REDSCRIPT_UTILS_SPINLOCK_H
#define REDSCRIPT_UTILS_SPINLOCK_H

#include <atomic>

#include "utils/Immovable.h"
#include "utils/NonCopyable.h"

namespace RedScript::Utils
{
class SpinLock final : public Immovable, public NonCopyable
{
    struct
    {
        std::atomic_uint32_t _users;
        std::atomic_uint32_t _ticket;
    };

private:
    static constexpr auto ACQ_REL = std::memory_order_acq_rel;
    static constexpr auto ACQUIRE = std::memory_order_acquire;

public:
    SpinLock() : _users(0), _ticket(0) {}

public:
    inline void release(void) { _ticket.fetch_add(1, ACQ_REL); }
    inline void acquire(void)
    {
        auto id = _users.fetch_add(1, ACQ_REL);
        while (id != _ticket.load(ACQUIRE));
    }

public:
    class Scope
    {
        SpinLock &_lock;

    public:
       ~Scope() { _lock.release(); }
        Scope(SpinLock &lock) : _lock(lock) { _lock.acquire(); }

    };
};
}

#endif /* REDSCRIPT_UTILS_SPINLOCK_H */
