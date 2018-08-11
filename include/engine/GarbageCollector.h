#ifndef REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H
#define REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H

#include <atomic>
#include <cstdio>
#include <cstdint>

#include "runtime/ReferenceCounted.h"

namespace RedScript::Engine
{
struct __attribute__((aligned(16))) GCNode
{
    static constexpr int GC_UNTRACKED   = -1;
    static constexpr int GC_REACHABLE   = -2;
    static constexpr int GC_UNREACHABLE = -3;

public:
    static constexpr int GC_YOUNG       = 0;
    static constexpr int GC_OLD         = 1;
    static constexpr int GC_PERM        = 2;

public:
    GCNode *prev;
    GCNode *next;

public:
    int32_t gen = GC_UNTRACKED;
    int32_t ref = GC_UNTRACKED;

public:
    GCNode() : prev(this), next(this) {}

};

class GCObject final : protected GCNode
{
    friend class Generation;
    friend class GarbageCollector;

public:
    GCObject() = default;
   ~GCObject() { untrack(); }

public:
    void track(void);
    void untrack(void);
    bool isTracked(void) const { return ref != GC_UNTRACKED; }

};

class GarbageCollector final
{
    static void free(void *obj);
    static void *alloc(size_t size);
    friend class Runtime::ReferenceCounted;

public:
    enum class CollectionMode
    {
        Fast,
        Full,
    };

public:
    static void shutdown(void);
    static size_t collect(CollectionMode mode);

};
}

#endif /* REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H */
