#ifndef REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H
#define REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H

#include <atomic>
#include <cstdio>
#include <cstdint>

#include "lockfree/DoublyLinkedList.h"

namespace RedScript::Engine
{
class GCObject final
{
    static constexpr int GC_UNTRACK     = -1;
    static constexpr int GC_REACHABLE   = -2;
    static constexpr int GC_UNREACHABLE = -3;

public:
    static constexpr int GC_YOUNG       = 0;
    static constexpr int GC_OLD         = 1;
    static constexpr int GC_PERM        = 2;
    static constexpr int GC_GEN_COUNT   = 3;

private:
    GCObject *_prev = nullptr;
    GCObject *_next = nullptr;

private:
    size_t _size;
    int32_t _level;
    std::atomic_int32_t _refCount;

private:
    friend class GarbageCollector;
    static_assert(std::atomic_int32_t::is_always_lock_free, "Non atomic reference counter");

public:
   ~GCObject() { untrack(); }
    GCObject(size_t size) : _size(size), _level(GC_UNTRACK), _refCount(GC_UNTRACK) {}

public:
    void track(void);
    void untrack(void);

public:
    bool isTracked(void) const { return _refCount.load() != GC_UNTRACK; }

};

struct GarbageCollector
{
    static int gc(void);
    static void freeObject(void *obj);
    static void *allocObject(size_t size);
};
}

#endif /* REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H */
