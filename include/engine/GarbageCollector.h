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

private:
    int32_t _gen;
    std::atomic_int _refCount;
    LockFree::DoublyLinkedList<GCObject *>::iterator _iter;

private:
    /* to align the object size with 16-bytes */
    uintptr_t __not_used_just_for_alignment__;

private:
    friend class Generation;
    friend class GarbageCollector;

public:
   ~GCObject() { untrack(); }
    GCObject() : _gen(GC_UNTRACK), _refCount(GC_UNTRACK) {}

public:
    void track(void);
    void untrack(void);

public:
    bool isTracked(void) const { return _refCount.load() != GC_UNTRACK; }

};

struct GarbageCollector
{
    static void shutdown(void);
    static void initialize(size_t young, size_t old, size_t perm);

public:
    static int gc(void);
    static void freeObject(void *obj);
    static void *allocObject(size_t size);

};
}

#endif /* REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H */
