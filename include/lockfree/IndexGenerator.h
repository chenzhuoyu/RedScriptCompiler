#ifndef REDSCRIPT_LOCKFREE_INDEXGENERATOR_H
#define REDSCRIPT_LOCKFREE_INDEXGENERATOR_H

#include <cstdio>
#include <cstdint>

#include <atomic>
#include <stdexcept>

#include "engine/Memory.h"

#include "utils/Immovable.h"
#include "utils/NonCopyable.h"

namespace RedScript::LockFree
{
class IndexGenerator final : private Utils::Immovable, private Utils::NonCopyable
{
    int *_bucket;
    size_t _maxCount;
    std::atomic_int _next;
    std::atomic_size_t _count;

public:
    ~IndexGenerator()
    {
        /* destroy the bucket */
        delete[] _bucket;
    }

public:
    explicit IndexGenerator(size_t maxCount) : _next(0), _count(0), _maxCount(maxCount)
    {
        /* restrict maximum ID count to 2 billion, which already takes 8G of RAM just to hold the ID bucket */
        if (_maxCount > 2147483648ul)
            throw std::overflow_error("ID is too large to generate");

        /* create the bucket, the last node doesn't have next node */
        _bucket = new int [_maxCount];
        _bucket[_maxCount - 1] = -1;

        /* and initialize every other node */
        for (size_t i = 0; i < _maxCount - 1; i++)
            _bucket[i] = static_cast<int>(i);
    }

public:
    size_t count(void) const { return _count.load(); }
    size_t capacity(void) const { return _maxCount; }

public:
    int acquire(void) noexcept
    {
        /* load next ID and current counter */
        int head;
        int next = _next.load();
        size_t count = _count.load();

        /* check ID counter and increase it */
        do if (count >= _maxCount) return -1;
        while (!_count.compare_exchange_strong(count, count + 1));

        /* detach the first node and update next ID */
        do head = _bucket[next];
        while (!_next.compare_exchange_strong(next, head));
        return next;
    }

public:
    void release(int id)
    {
        /* check for ID */
        if (id < 0 || id >= _maxCount)
            throw std::out_of_range("Invalid ID");

        /* load the old head */
        int next = _next.load();

        /* update the header pointer with CAS */
        do _bucket[id] = next;
        while (!_next.compare_exchange_strong(next, id));

        /* update the ID counter */
        if (!(_count--))
            throw std::underflow_error("ID pool underflow");
    }
};
}

#endif /* REDSCRIPT_LOCKFREE_INDEXGENERATOR_H */
