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
    union
    {
        struct
        {
            uint16_t _reads;
            uint16_t _writes;
            uint16_t _tickets;
        };

        uint64_t _all;
        uint32_t _access;
    };

private:
    size_t _rlocks;
    Engine::Thread *_owner;

public:
    RWLock() : _all(0), _rlocks(0), _owner(nullptr) {}

public:
    size_t reads(void) const { return _rlocks; }
    Engine::Thread *owner(void) const { return _owner; }

public:
    void readLock(void)
    {
        auto tk = __sync_fetch_and_add(&_tickets, 1);
        while (tk != _reads);
        __sync_add_and_fetch(&_reads, 1);
    }

public:
    void writeLock(void)
    {
        auto tk = __sync_fetch_and_add(&_tickets, 1);
        while (tk != _writes);
    }

public:
    void readUnlock(void) { __sync_add_and_fetch(&_writes, 1); }
    void writeUnlock(void) { _access = (_reads + 1u) | ((_writes + 1u) << 16u); }

public:
    class Read
    {
        RWLock &_lock;

    public:
        ~Read()
        {
            __sync_sub_and_fetch(&(_lock._rlocks), 1);
            _lock.readUnlock();
        }

    public:
        Read(RWLock &lock) : _lock(lock)
        {
            _lock.readLock();
            __sync_add_and_fetch(&(_lock._rlocks), 1);
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
            _lock.writeUnlock();
        }

    public:
        Write(RWLock &lock) : _lock(lock)
        {
            _lock.writeLock();
            _lock._owner = Engine::Thread::self();
        }
    };
};
}

#endif /* REDSCRIPT_UTILS_READWRITELOCK_H */
