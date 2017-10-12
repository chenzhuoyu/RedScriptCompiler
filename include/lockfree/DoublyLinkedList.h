#ifndef REDSCRIPT_LOCKFREE_DOUBLYLINKEDLIST_H
#define REDSCRIPT_LOCKFREE_DOUBLYLINKEDLIST_H

#include <new>
#include <cstdint>
#include <cstdlib>

#include "lockfree/HazardPointer.h"

namespace RedScript::LockFree
{
template <typename T>
class DoublyLinkedList
{
    struct Node;
    struct Link;
    typedef HazardPointer<Node, Link> Pointer;

private:
    struct Link : public HazardLink
    {
        static constexpr uintptr_t DEL_MASK = 1;
        static constexpr uintptr_t PTR_MASK = ~DEL_MASK;

    private:
        static inline Node *addr2node(uintptr_t node) { return reinterpret_cast<Node *>(node); }
        static inline uintptr_t node2addr(HazardNode *node) { return reinterpret_cast<uintptr_t>(node); }

    public:
        Link() : Link(nullptr) {}
        Link(Node *node) : Link(node, false) {}
        Link(Node *node, bool del) : HazardLink(addr2node(node2addr(node) | (del ? DEL_MASK : 0))) {}

    public:
        bool del(void) const { return bool(node2addr(this->data.load()) & DEL_MASK); }
        Node *ptr(void) const { return addr2node(node2addr(this->data.load()) & PTR_MASK); }

    };

private:
    struct Node : public HazardNode
    {
        T val;
        Link prev;
        Link next;

    public:
        void update(void) override
        {
            Node *prev, *prev2;
            Node *next, *next2;

            /* process the `prev` side */
            while ((prev = Pointer::retain(this->prev)))
            {
                /* already deleted */
                if (!prev->prev.del())
                {
                    Pointer::release(prev);
                    break;
                }

                /* move to it's predecessor */
                prev2 = Pointer::retain(prev->prev);
                this->prev.referenceCAS(Link(prev, true), Link(prev2, true));

                /* release references */
                Pointer::release(prev);
                Pointer::release(prev2);
            }

            /* process the `next` side */
            while ((next = Pointer::retain(this->next)))
            {
                /* already deleted */
                if (!next->next.del())
                {
                    Pointer::release(next);
                    break;
                }

                /* move to it's predecessor */
                next2 = Pointer::retain(next->next);
                this->next.referenceCAS(Link(next, true), Link(next2, true));

                /* release references */
                Pointer::release(next);
                Pointer::release(next2);
            }
        }

    public:
        void terminate(bool isConcurrent) override
        {
            if (!isConcurrent)
            {
                prev.setUnsafe(Link(nullptr, true));
                next.setUnsafe(Link(nullptr, true));
            }
            else
            {
                prev.referenceCAS(Link(prev.ptr()), Link(nullptr, true));
                next.referenceCAS(Link(next.ptr()), Link(nullptr, true));
            }
        }
    };
};
}

#undef CAS

#endif /* REDSCRIPT_LOCKFREE_DOUBLYLINKEDLIST_H */
