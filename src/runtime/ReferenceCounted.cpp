#include "engine/GarbageCollector.h"
#include "runtime/ReferenceCounted.h"

#define TO_GC(obj)  (reinterpret_cast<Engine::GCObject *>(reinterpret_cast<uintptr_t>(obj)) - 1)

namespace RedScript::Runtime
{
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
    /* allocate from garbage collector */
    return Engine::GarbageCollector::alloc(size);
}

void ReferenceCounted::operator delete(void *self)
{
    /* just throw the object into GC
     * should be compatible with nullptr */
    if (self != nullptr)
        Engine::GarbageCollector::free(self);
}
}
