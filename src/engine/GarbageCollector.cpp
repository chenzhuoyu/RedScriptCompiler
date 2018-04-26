#include <new>
#include <atomic>
#include <memory>
#include <cstdlib>
#include <stdexcept>
#include <shared_mutex>

#include "engine/Memory.h"
#include "engine/Thread.h"
#include "engine/GarbageCollector.h"

#include "runtime/Object.h"

namespace RedScript::Engine
{
struct Generation
{
    size_t size;
    std::atomic_size_t used;

private:
//    LockFree::DoublyLinkedList<GCObject *> _objects;

public:
    Generation(size_t size) : size(size), used(0) {}

public:
    void addObject(GCObject *object)
    {
        used += Memory::sizeOf(object);
//        _objects.push_back(object, &(object->_iter));
    }

public:
    void removeObject(GCObject *object)
    {
        used -= Memory::sizeOf(object);
//        _objects.erase(object->_iter);
    }
};

/* GC generations */
static std::vector<std::unique_ptr<Generation>> _generations;

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
        _gen = GC_YOUNG;
        _generations[GC_YOUNG]->addObject(this);
    }
}

void GCObject::untrack(void)
{
    /* update the reference counter to untracked state */
    if (atomicSetNotEquals(_refCount, GC_UNTRACK))
    {
        /* verify GC generation */
        if ((_gen > GC_PERM) || (_gen < GC_YOUNG))
            throw std::out_of_range("Invalid GC generation");

        /* remove from generations */
        _generations[_gen]->removeObject(this);
        _gen = GC_UNTRACK;
    }
}

/*** GarbageCollector implementations ***/

void GarbageCollector::shutdown(void)
{
    _generations.clear();
    _generations.shrink_to_fit();
}

void GarbageCollector::initialize(size_t young, size_t old, size_t perm)
{
    _generations.emplace_back(std::make_unique<Generation>(young));
    _generations.emplace_back(std::make_unique<Generation>(old));
    _generations.emplace_back(std::make_unique<Generation>(perm));
}

int GarbageCollector::gc(void)
{
    /* check for initialization */
    if (_generations.empty())
        throw std::runtime_error("GarbageCollector not initialized");

    // TODO: collect dead objects
    return 0;
}

void GarbageCollector::freeObject(void *obj)
{
    /* check for initialization */
    if (_generations.empty())
        throw std::runtime_error("GarbageCollector not initialized");

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

    /* check for initialization */
    if (_generations.empty())
        throw std::runtime_error("GarbageCollector not initialized");

    /* allocate memory for GC objects, and initialize it */
    void *mem = Memory::alloc(size + sizeof(GCObject));
    GCObject *result = Memory::construct<GCObject>(mem);

    /* skip the GC header */
    return reinterpret_cast<void *>(result + 1);
}
}
