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
    size_t _size;
    int32_t _level;

private:
    std::atomic_int _refCount;
    LockFree::DoublyLinkedList<GCObject *>::iterator _iter;

private:
    /* to align the object size with 16-bytes */
    uint32_t __not_used_just_for_alignment_1__;
    uintptr_t __not_used_just_for_alignment_2__;

private:
    friend class Generation;
    friend class GarbageCollector;

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
    static void init(void);
    static void shutdown(void);

public:
    static int gc(void);
    static void freeObject(void *obj);
    static void *allocObject(size_t size);

};
}

#endif /* REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H */
