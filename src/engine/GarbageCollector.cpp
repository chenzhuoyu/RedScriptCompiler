#include <new>
#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <shared_mutex>

#include "utils/Pointers.h"

#include "engine/Thread.h"
#include "engine/GarbageCollector.h"

#define GC_YOUNG                0
#define GC_OLD                  1
#define GC_PERM                 2
#define GC_GEN_COUNT            3

#define GC_UNTRACK              -1
#define GC_REACHABLE            -2
#define GC_UNREACHABLE          -3

namespace RedScript::Engine
{
namespace
{
struct Generation
{
    size_t size;
    size_t used;
    GCObject list;
    std::shared_mutex lock;

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
static Generation _generations[GC_GEN_COUNT] = {
    Generation(1 * 1024 * 1024 * 1024),    /* Young,   1G */
    Generation(     512 * 1024 * 1024),    /* Old  , 512M */
    Generation(     128 * 1024 * 1024),    /* Perm , 128M */
};

/*** GCObject implementations ***/

template <typename T>
static inline bool atomicSetNotEquals(std::atomic<T> &value, T &&newValue)
{
    /* atomic exchange */
    return value.exchange(newValue) != newValue;
}

template <typename T>
static inline bool atomicCompareAndSwap(std::atomic<T> &value, T &&oldValue, T &&newValue)
{
    /* atomic strong CAS */
    return value.compare_exchange_strong(oldValue, newValue);
}

void GCObject::add(void)
{
    /* CAS the reference count to check if already added */
    if (atomicCompareAndSwap(_refCount, GC_UNTRACK, GC_REACHABLE))
    {
        _generations[GC_YOUNG].addObject(this);
        _level = GC_YOUNG;
    }
}

void GCObject::remove(void)
{
    /* update the reference counter to untracked state */
    if (atomicSetNotEquals(_refCount, GC_UNTRACK))
    {
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

    /* remove from GC track list and free back to system */
    object->remove();
    free(object);
}

void *GarbageCollector::allocObject(size_t size)
{
    /* check GC object size at compile time */
    static_assert(
        (sizeof(GCObject) % 16) == 0,
        "Misaligned GC object, size must be aligned to 16-bytes"
    );

    void *mem;
    GCObject *result;

    /* allocate memory for GC objects, align with 16-bytes */
    if (posix_memalign(&mem, 16, size + sizeof(GCObject)))
        throw std::bad_alloc();

    /* convert to GC object */
    result = static_cast<GCObject *>(mem);
    result->_size = size;
    result->_level = GC_UNTRACK;
    result->_refCount = GC_UNTRACK;

    /* skip the GC header */
    return reinterpret_cast<void *>(result + 1);
}
}
