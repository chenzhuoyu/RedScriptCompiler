#include <new>
#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <shared_mutex>

#include "utils/Pointers.h"

#include "engine/Memory.h"
#include "engine/Thread.h"
#include "engine/GarbageCollector.h"

namespace RedScript::Engine
{
namespace
{
struct Generation
{
    size_t size;
    size_t used;

public:
    Generation(size_t size) : size(size), used(0) {}

public:
    void addObject(GCObject *object)
    {

    }

public:
    void removeObject(GCObject *object)
    {

    }
};
}

/* GC generations */
static Generation _generations[GCObject::GC_GEN_COUNT] = {
    Generation(1 * 1024 * 1024 * 1024),    /* Young,   1G */
    Generation(     512 * 1024 * 1024),    /* Old  , 512M */
    Generation(     128 * 1024 * 1024),    /* Perm , 128M */
};

/*** GCObject implementations ***/

template <typename T>
static inline bool atomicSetNotEquals(std::atomic<T> &value, T newValue)
{
    /* atomic exchange */
    return value.exchange(newValue) != newValue;
}

template <typename T>
static inline bool atomicCompareAndSwap(std::atomic<T> &value, T oldValue, T newValue)
{
    /* atomic strong CAS */
    return value.compare_exchange_strong(oldValue, newValue);
}

void GCObject::track(void)
{
    /* CAS the reference count to check if already added */
    if (atomicCompareAndSwap(_refCount, GC_UNTRACK, GC_REACHABLE))
    {
        _level = GC_YOUNG;
        _generations[GC_YOUNG].addObject(this);
    }
}

void GCObject::untrack(void)
{
    /* update the reference counter to untracked state */
    if (atomicSetNotEquals(_refCount, GC_UNTRACK))
    {
        /* verify GC level */
        if ((_level > GC_PERM) || (_level < GC_YOUNG))
            throw std::out_of_range("bad level");

        /* remove from generations */
        _generations[_level].removeObject(this);
        _level = GC_UNTRACK;
    }
}

/*** GarbageCollector implementations ***/

int GarbageCollector::gc(void)
{
    return 0;
}

void GarbageCollector::freeObject(void *obj)
{
    /* locate the real GC header */
    GCObject *object = static_cast<GCObject *>(obj) - 1;

    /* and destroy the GC object header */
    Memory::destroy(object);
    Memory::free(object);
}

void *GarbageCollector::allocObject(size_t size)
{
    /* check GC object size at compile time */
    static_assert(
        (sizeof(GCObject) % 16) == 0,
        "Misaligned GC object, size must be aligned to 16-bytes"
    );

    /* allocate memory for GC objects, and initialize it */
    void *mem = Memory::alloc(size + sizeof(GCObject));
    GCObject *result = Memory::construct<GCObject>(mem, size);

    /* skip the GC header */
    return reinterpret_cast<void *>(result + 1);
}
}
