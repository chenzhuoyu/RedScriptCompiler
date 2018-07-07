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
            std::atomic<uint16_t> _reads;
            std::atomic<uint16_t> _writes;
            std::atomic<uint16_t> _tickets;
        };

        std::atomic<uint32_t> _rw;
        std::atomic<uint64_t> _all;
    };

private:
    Engine::Thread *_owner;
    std::atomic_size_t _rlocks;

public:
    RWLock() : _all(0), _owner(nullptr), _rlocks(0) {}

public:
    size_t reads(void) const { return _rlocks; }
    Engine::Thread *owner(void) const { return _owner; }

public:
    void readLock(void)
    {
        uint16_t tk = _tickets++;
        while (tk != _reads);
        _reads++;
    }

public:
    void writeLock(void)
    {
        uint16_t tk = _tickets++;
        while (tk != _writes);
    }

public:
    void readUnlock(void) { _writes++; }
    void writeUnlock(void) { _rw = (_reads + 1) | ((_writes + 1) << 16); }

public:
    class Read
    {
        RWLock &_lock;

    public:
        ~Read()
        {
            _lock._rlocks--;
            _lock.readUnlock();
        }

    public:
        Read(RWLock &lock) : _lock(lock)
        {
            _lock.readLock();
            _lock._rlocks++;
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
            _lock._owner = Engine::Thread::current();
        }
    };
};
}

#endif /* REDSCRIPT_UTILS_READWRITELOCK_H */
