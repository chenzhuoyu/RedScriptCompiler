#include <new>
#include <mutex>
#include <memory>
#include <cstdlib>
#include <stdexcept>

#include "utils/SpinLock.h"
#include "engine/Memory.h"
#include "engine/GarbageCollector.h"

#define TO_GC(obj)      (reinterpret_cast<GCObject *>(obj) - 1)
#define FROM_GC(obj)    (reinterpret_cast<Runtime::ReferenceCounted *>(obj + 1))

namespace RedScript::Engine
{
/* check GC object size at compile time */
static_assert(
    (sizeof(GCObject) % 16) == 0,
    "Misaligned GC object, size must be aligned to 16-bytes"
);

struct Generation
{
    size_t size;
    int32_t name;
    std::atomic_size_t used;

private:
    GCNode _head;
    Utils::SpinLock _spin;

public:
    Generation(int32_t name, size_t size) : name(name), size(size), used(0) {}

private:
    static inline GCNode *remove(GCNode *node)
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
        return node;
    }

private:
    static inline GCNode *append(GCNode *node, GCNode *head)
    {
        node->next = head;
        node->prev = head->prev;
        head->prev->next = node;
        head->prev = node;
        return node;
    }

private:
    inline void attach(GCNode *object)
    {
        Utils::SpinLock::Scope _(_spin);
        append(object, &_head)->gen = name;
    }

private:
    inline void detach(GCNode *object)
    {
        Utils::SpinLock::Scope _(_spin);
        remove(object)->gen = GCNode::GC_UNTRACKED;
    }

public:
    void addObject(GCNode *object)
    {
        used++;
        attach(object);
    }

public:
    void removeObject(GCNode *object)
    {
        used--;
        detach(object);
    }

private:
    inline void markObjects(GCNode *head)
    {
        /* lock before collecting */
        Utils::SpinLock::Scope _(_spin);

        /* update references */
        for (GCNode *p = _head.next; p != &_head; p = p->next)
            p->ref = FROM_GC(p)->refCount();

        /* subtract references */
        for (GCNode *p = _head.next; p != &_head; p = p->next)
        {
            /* traverse each registered object */
            FROM_GC(p)->referenceTraverse([](Runtime::ReferenceCounted *self)
            {
                /* try unreference the object, static object cannot be collected */
                if (!(self->isStatic()))
                    if (TO_GC(self)->ref > 0)
                        TO_GC(self)->ref--;
            });
        }

        /* move object back to reachable list */
        auto moveReachable = [&](Runtime::ReferenceCounted *self)
        {
            /* static object cannot be collected */
            if (!(self->isStatic()))
            {
                /* the object is reachable but "move unreachable" process
                 * hasn't yet gotton to it, so give it a dummy reference */
                if (!(TO_GC(self)->ref))
                    TO_GC(self)->ref = 1;

                /* marked as "unreachable", but actually it is reachable (because
                 * we are already reaching it), so move back to "reachable" list */
                else if (TO_GC(self)->ref == GCNode::GC_UNREACHABLE)
                    append(remove(TO_GC(self)), &_head);
            }
        };

        /* head of the list */
        GCNode *q = nullptr;
        GCNode *p = _head.next;

        /* move unreachable object into another list */
        while (p != &_head)
        {
            /* move to next node */
            q = p;
            p = p->next;

            /* if the reference dropped to zero, the
             * object **MAY** not reachable from outside */
            if (!(q->ref))
            {
                append(remove(q), head)->ref = GCNode::GC_UNREACHABLE;
                continue;
            }

            /* mark it as reachable, and scan it's referenced objects */
            q->ref = GCNode::GC_REACHABLE;
            FROM_GC(q)->referenceTraverse(moveReachable);
        }
    }

private:
    inline size_t freeObjects(GCNode *head)
    {
        size_t n = 0;
        GCNode *q = nullptr;
        typedef Runtime::Reference<Runtime::ReferenceCounted> ARCObject;

        /* collect every object */
        while ((q = head->next) != head)
        {
            /* clear the object */
            ARCObject::retain(FROM_GC(q))->referenceClear();

            /* if still alive, must be referenced by a object that
             * is about to be collected, so move it to the end of list */
            if (q != head->next)
                n++;
            else
                append(remove(q), head);
        }

        /* collected object count */
        return n;
    }

public:
    size_t move(Generation &target)
    {
        /* check for generations */
        if (name >= target.name)
            throw std::logic_error("move to younger generation is not allowed");

        /* lock both generations */
        size_t size;
        GCNode *tail;
        Utils::SpinLock::Scope self(_spin);
        Utils::SpinLock::Scope other(target._spin);

        /* empty list, nothing to move */
        if (_head.next == &_head)
            return 0;

        /* merge the two lists */
        tail = target._head.prev;
        tail->next = _head.next;
        tail->next->prev = tail;
        target._head.prev = _head.prev;
        target._head.prev->next = &(target._head);

        /* clear the current generation */
        _head.next = &_head;
        _head.prev = &_head;

        /* update the generation size */
        size = used.exchange(0);
        target.used.fetch_add(size);
        return size;
    }

public:
    size_t collect(void)
    {
        GCNode head;
        markObjects(&head);
        return freeObjects(&head);
    }
};

/* GC generations */
static Generation Generations[] = {
    Generation(GCNode::GC_YOUNG, 4096),
    Generation(GCNode::GC_OLD  , 256 ),
    Generation(GCNode::GC_PERM , 256 ),
};

/* garbage collector state */
static std::atomic_flag _isCollecting = false;
static std::atomic_size_t _residentObjects = 0;

/*** GCObject implementations ***/

void GCObject::track(void)
{
    /* check bucket size, perform a fast GC as necessary */
    if (Generations[GC_YOUNG].used >= Generations[GC_YOUNG].size)
        GarbageCollector::collect(GarbageCollector::CollectionMode::Fast);

    /* CAS the reference count to check if already added */
    if (__sync_bool_compare_and_swap(&ref, GC_UNTRACKED, GC_REACHABLE))
        Generations[GC_YOUNG].addObject(this);
}

void GCObject::untrack(void)
{
    /* update the reference counter to untracked state */
    if (__sync_val_compare_and_swap(&ref, ref, GC_UNTRACKED) != GC_UNTRACKED)
    {
        if ((gen >= GC_YOUNG) && (gen <= GC_PERM))
            Generations[gen].removeObject(this);
        else
            throw std::out_of_range("Invalid GC generation");
    }
}

/*** GarbageCollector implementations ***/

void GarbageCollector::free(void *obj)
{
    Memory::destroy(TO_GC(obj));
    Memory::free(TO_GC(obj));
}

void *GarbageCollector::alloc(size_t size)
{
    size += sizeof(GCObject);
    return FROM_GC(Memory::construct<GCObject>(Memory::alloc(size)));
}

size_t GarbageCollector::collect(CollectionMode mode)
{
    /* check the collecting flags */
    if (_isCollecting.test_and_set())
        return 0;

    /* calculate generation count */
    size_t n = 0;
    size_t count = sizeof(Generations) / sizeof(Generations[0]);

    /* check for collection mode */
    switch (mode)
    {
        case CollectionMode::Full:
        {
            /* collect from younger to older generations */
            for (size_t i = 0; i < count; i++)
            {
                /* collect this generation */
                n += Generations[i].collect();

                /* move objects to next generation if needed */
                if (i < count - 1)
                    Generations[i].move(Generations[i + 1]);
            }

            /* reset resident object counter */
            _residentObjects = 0;
            break;
        }

        case CollectionMode::Fast:
        {
            /* find the oldest generation that exceeds the threshold, objects in
             * that generation and generations younger than that will be collected */
            for (size_t i = count, j = 0; i > 0; i--)
            {
                /* "j > 0" means collection already been triggered by an older generation,
                 * which means all generations younger than that will also need to be collected */
                if ((j > 0) || (Generations[i - 1].used >= Generations[i - 1].size))
                {
                    /* not the last generation (PERM generation) */
                    if (i != count)
                    {
                        n += Generations[i - 1].collect();
                        _residentObjects += Generations[i - 1].move(Generations[i]);
                    }

                    /* too many resident objects, force a full collection */
                    else if (_residentObjects >= Generations[i - 1].size / 4)
                    {
                        n += Generations[i - 1].collect();
                        _residentObjects = 0;
                    }

                    /* update collected generations */
                    j++;
                }
            }

            break;
        }
    }

    /* clear the collecting flags */
    _isCollecting.clear();
    return n;
}
}
