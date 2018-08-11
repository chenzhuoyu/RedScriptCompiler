#ifndef REDSCRIPT_UTILS_READWRITELOCK_H
#define REDSCRIPT_UTILS_READWRITELOCK_H

#include <atomic>

#include "utils/Immovable.h"
#include "utils/NonCopyable.h"

namespace RedScript::Utils
{
class RWLock final : public Immovable, public NonCopyable
{
    union
    {
        struct
        {
            std::atomic_uint32_t _reads;
            std::atomic_uint32_t _writes;
            std::atomic_uint32_t _tickets;
        };

        std::atomic_uint64_t _access;
    };

private:
    static constexpr auto ACQ_REL = std::memory_order_acq_rel;
    static constexpr auto ACQUIRE = std::memory_order_acquire;
    static constexpr auto RELEASE = std::memory_order_release;

public:
    RWLock() : _reads(0), _writes(0), _tickets(0) {}

public:
    inline void readLock(void)
    {
        auto tk = _tickets.fetch_add(1, ACQ_REL);
        while (tk != _reads.load(ACQUIRE));
        _reads.fetch_add(1, ACQ_REL);
    }

public:
    inline void writeLock(void)
    {
        auto tk = _tickets.fetch_add(1, ACQ_REL);
        while (tk != _writes.load(ACQUIRE));
    }

public:
    inline void readUnlock(void) { _writes.fetch_add(1, ACQ_REL); }
    inline void writeUnlock(void) { _access.store((_reads.load(ACQUIRE) + 1ul) | ((_writes.load(ACQUIRE) + 1ul) << 32ul), RELEASE); }

public:
    class Read
    {
        RWLock &_lock;

    public:
       ~Read() { _lock.readUnlock(); }
        Read(RWLock &lock) : _lock(lock) { _lock.readLock(); }

    };

public:
    class Write
    {
        RWLock &_lock;

    public:
       ~Write() { _lock.writeUnlock(); }
        Write(RWLock &lock) : _lock(lock) { _lock.writeLock(); }

    };
};
}

#endif /* REDSCRIPT_UTILS_READWRITELOCK_H */
