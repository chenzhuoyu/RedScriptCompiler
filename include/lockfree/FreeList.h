#ifndef REDSCRIPT_LOCKFREE_FREELIST_H
#define REDSCRIPT_LOCKFREE_FREELIST_H

#include <atomic>
#include "utils/Preprocessor.h"

namespace RedScript::LockFree
{
class FreeList
{
    struct Node;
    struct Pointer
    {
        Node *ptr;
        uintptr_t tag;

    public:
        Pointer() : ptr(nullptr), tag(0) {}
        Pointer(Node *ptr) : ptr(ptr), tag(0) {}

    };

public:
    struct Node
    {
        std::atomic<Node *> next;
        Node() { next.store(nullptr, std::memory_order_release); }
    };

private:
    std::atomic<Pointer> _head;
    static_assert(std::atomic<Pointer>::is_always_lock_free);

public:
   ~FreeList() { clear(); }
    FreeList() : _head(Pointer()) {}

public:
    bool isEmpty(void) const
    {
        Pointer ptr = _head.load(std::memory_order_relaxed);
        return !(ptr.ptr);
    }

public:
    Node *pop(void)
    {
        Pointer next;
        Pointer head = _head.load(std::memory_order_relaxed);

        while (head.ptr)
        {
            next.tag = head.tag + 1;
            next.ptr = head.ptr->next.load(std::memory_order_relaxed);

            if (RSPP_LIKELY(_head.compare_exchange_weak(head, next, std::memory_order_release, std::memory_order_acquire)))
                break;
        }

        return head.ptr;
    }

public:
    void push(Node *node)
    {
        Pointer next = node;
        Pointer head = _head.load(std::memory_order_relaxed);

        do
        {
            next.tag = head.tag + 1;
            node->next.store(head.ptr, std::memory_order_relaxed);
        } while (RSPP_UNLIKELY(!_head.compare_exchange_weak(head, next, std::memory_order_release, std::memory_order_acquire)));
    }

public:
    void clear(void)
    {
        Node *next;
        Node *head = _head.exchange(nullptr, std::memory_order_relaxed).ptr;

        while (head)
        {
            next = head->next.load(std::memory_order_relaxed);
            delete head;
            head = next;
        }
    }
};
}

#endif /* REDSCRIPT_LOCKFREE_FREELIST_H */
