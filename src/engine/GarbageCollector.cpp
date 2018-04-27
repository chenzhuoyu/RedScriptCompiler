#include <new>
#include <mutex>
#include <memory>
#include <cstdlib>
#include <stdexcept>

#include "engine/Memory.h"
#include "engine/GarbageCollector.h"

#include "utils/SpinLock.h"
#include "runtime/Object.h"

namespace RedScript::Engine
{
struct Generation
{
    size_t size;
    std::atomic_size_t used;

private:
    GCObject _head;
    Utils::SpinLock _spin;

public:
    Generation(size_t size) : size(size), used(0)
    {
        _head._prev = &_head;
        _head._next = &_head;
    }

private:
    inline void attach(GCObject *object)
    {
        Utils::SpinLock::Scope _(_spin);
        object->_next = &_head;
        object->_prev = _head._prev;
        _head._prev->_next = object;
        _head._prev = object;
    }

private:
    inline void detach(GCObject *object)
    {
        Utils::SpinLock::Scope _(_spin);
        object->_prev->_next = object->_next;
        object->_next->_prev = object->_prev;
    }

public:
    void addObject(GCObject *object)
    {
        attach(object);
        used += Memory::sizeOf(object);
    }

public:
    void removeObject(GCObject *object)
    {
        detach(object);
        used -= Memory::sizeOf(object);
    }
};

/* GC generations */
static std::vector<std::unique_ptr<Generation>> _generations;

/*** GCObject implementations ***/

void GCObject::track(void)
{
    /* CAS the reference count to check if already added */
    if (__sync_bool_compare_and_swap(&_ref, GC_UNTRACK, GC_REACHABLE))
    {
        _gen = GC_YOUNG;
        _generations[GC_YOUNG]->addObject(this);
    }
}

void GCObject::untrack(void)
{
    /* update the reference counter to untracked state */
    if (__sync_val_compare_and_swap(&_ref, _ref, GC_UNTRACK) != GC_UNTRACK)
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
