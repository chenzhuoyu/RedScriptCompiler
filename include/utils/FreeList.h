#ifndef REDSCRIPT_UTILS_FREELIST_H
#define REDSCRIPT_UTILS_FREELIST_H

#include <atomic>
#include <cstddef>

namespace RedScript::Utils
{
template <typename T, typename Alloc>
struct FreeList
{
    struct Node;
    struct Next
    {
        Node *ptr;
        size_t tag;

    public:
        Next() : Next(nullptr, 0) {}
        Next(Node *ptr, size_t counter) : ptr(ptr), tag(counter) {}

    };

public:
    typedef std::atomic<Next> Link;
    static_assert(Link::is_always_lock_free, "Non-atomic link");

public:
    struct Node
    {
        T *data;
        Link next;
    };

public:
    Link head;
    FreeList() : head({nullptr, 0}) {}

public:
    inline Node *alloc(void)
    {
        /* current list head */
        Next link;
        Next next = head.load(std::memory_order_consume);

        for (;;)
        {
            /* no more items in free-list */
            if (next.ptr == nullptr)
                break;

            /* setup new link */
            link.tag = next.tag + 1;
            link.ptr = next.ptr->next.load(std::memory_order_consume).ptr;

            /* CAS with the list head, set with next node */
            if (head.compare_exchange_weak(next, link))
                return next.ptr;
        }

        /* construct a new node */
        return new Node{ Alloc::alloc(), Next() };
    }

public:
    inline void free(Node *node)
    {
        /* current list head */
        Next link;
        Next next = head.load(std::memory_order_consume);

        for (;;)
        {
            /* link the `next` node of `node` to list head */
            link.ptr = node;
            link.tag = next.tag + 1;
            node->next.store(next, std::memory_order_release);

            /* CAS with the list head, set with new head */
            if (head.compare_exchange_weak(next, link))
                break;
        }
    }

public:
    inline void clear(void)
    {
        /* current list head */
        Next next = head.load(std::memory_order_consume);
        Node *node;

        /* CAS with the list head, set to NULL */
        do node = next.ptr;
        while (!(head.compare_exchange_weak(next, Next())));

        /* release every item */
        while (node)
        {
            /* save the next node first */
            next = node->next.load(std::memory_order_consume);
            Alloc::free(node->data);

            /* destroy the node */
            delete node;
            node = next.ptr;
        }
    }
};
}

#endif /* REDSCRIPT_UTILS_FREELIST_H */
