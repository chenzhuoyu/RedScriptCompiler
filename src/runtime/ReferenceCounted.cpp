#include "engine/GarbageCollector.h"
#include "runtime/ReferenceCounted.h"

namespace RedScript::Runtime
{
/* thread-local, so no atomic operations needed */
static thread_local bool _isStaticObject = true;

ReferenceCounted::ReferenceCounted() : _refCount(0)
{
    _isStatic = _isStaticObject;
    _isStaticObject = true;
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
    /* should compatible with nullptr */
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
