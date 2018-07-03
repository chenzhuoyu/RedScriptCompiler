#include "engine/GarbageCollector.h"
#include "runtime/ReferenceCounted.h"

#define TO_GC(obj)  (reinterpret_cast<Engine::GCObject *>(reinterpret_cast<uintptr_t>(obj)) - 1)

namespace RedScript::Runtime
{
/* thread-local, so no atomic operations required */
static thread_local bool _isStaticObject = true;

ReferenceCounted::ReferenceCounted() : _refCount(1)
{
    _isStatic = _isStaticObject;
    _isStaticObject = true;
}

void ReferenceCounted::track(void) const
{
    if (!_isStatic)
        TO_GC(this)->track();
}

void ReferenceCounted::untrack(void) const
{
    if (!_isStatic)
        TO_GC(this)->untrack();
}

bool ReferenceCounted::isTracked(void) const
{
    /* static objects are always not tracked */
    return !_isStatic && TO_GC(this)->isTracked();
}

void *ReferenceCounted::operator new(size_t size)
{
    /* check for minimun required size */
    if (size < sizeof(ReferenceCounted))
        throw std::bad_alloc();

    /* mark as dynamic created object, and allocate new object from GC */
    _isStaticObject = false;
    return Engine::GarbageCollector::allocObject(size);
}

void ReferenceCounted::operator delete(void *self)
{
    /* should be compatible with nullptr */
    if (self)
    {
        /* check for static object */
        if (static_cast<ReferenceCounted *>(self)->_isStatic)
        {
            fprintf(stderr, "*** FATAL: free'ing static object %p\n", self);
            abort();
        }

        /* just throw the object into GC */
        Engine::GarbageCollector::freeObject(self);
    }
}
}
