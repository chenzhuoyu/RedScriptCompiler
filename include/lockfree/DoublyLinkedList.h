#ifndef REDSCRIPT_LOCKFREE_DOUBLYLINKEDLIST_H
#define REDSCRIPT_LOCKFREE_DOUBLYLINKEDLIST_H

#include <new>
#include <atomic>
#include <cstdint>
#include <cstdlib>

namespace RedScript::LockFree
{
template <typename T>
class DoublyLinkedList
{
    struct Node;
    struct Link
    {
        Node *node;

    public:
        static constexpr uintptr_t DEL_MASK = 1;
        static constexpr uintptr_t PTR_MASK = ~DEL_MASK;

    private:
        static inline Node *addr2node(uintptr_t node) { return reinterpret_cast<Node *>(node); }
        static inline uintptr_t node2addr(Node *node) { return reinterpret_cast<uintptr_t>(node); }

    public:
        Link() : Link(nullptr) {}
        Link(Node *node) : Link(node, false) {}
        Link(Node *node, bool del) : node(addr2node(node2addr(node) | (del ? DEL_MASK : 0))) {}

    public:
        bool del(void) const { return bool(node2addr(this->node) & DEL_MASK); }
        Node *ptr(void) const { return addr2node(node2addr(this->node) & PTR_MASK); }

    public:
        bool operator!=(const Link &other) const { return node != other.node; }
        bool operator==(const Link &other) const { return node == other.node; }

    };

private:
    struct Node
    {
        std::atomic<Link> prev;
        std::atomic<Link> next;

    public:
        T data;
        std::atomic_uint64_t ref;

    public:
        Node(const T &data) : ref(1), data(data) {}

    };

public:
    typedef T value_type;

private:
    inline Node *ref(Node *node)
    {
        if (node) node->ref++;
        return node;
    }

private:
    inline Node *ref(std::atomic<Link> &link)
    {
        Link data = link.load();
        return data.del() ? nullptr : ref(data.ptr());
    }

private:
    inline void unref(Node *node)
    {
        if (node && --node->ref == 0)
        {
            Link prev = node->prev.load();
            Link next = node->next.load();

            /* unref both link if not deleted */
            if (!prev.del()) unref(prev.ptr());
            if (!next.del()) unref(next.ptr());
            delete node;
        }
    }

private:
    inline bool casLink(std::atomic<Link> &link, Link cmp, const Link &value)
    {
        /* compare and swap link with delete flag */
        return link.compare_exchange_strong(cmp, value);
    }

private:
    Node *_head = nullptr;
    Node *_tail = nullptr;
    std::atomic_size_t _count = 0;

private:
    void markDelete(Node *node)
    {
        Link link;
        do link = node->prev.load();
        while (!(link.del() || casLink(node->prev, link, Link(link.ptr(), true))));
    }

private:
    void pushComplete(Node *node, Node *next)
    {
        for (;;)
        {
            /* load the previous link */
            Link link = next->prev.load();

            /* link was removed, or updated by another thread */
            if (link.del() || (node->next.load() != Link(next)))
                break;

            /* perform CAS operations */
            if (casLink(next->prev, link, Link(node)))
            {
                ref(node);
                unref(link.ptr());

                /* previous node was removed, fix the `prev` link */
                if (node->prev.load().del())
                    unref(updatePrevLink(ref(node), next));

                /* CAS successful */
                break;
            }
        }

        /* unref both nodes */
        unref(next);
        unref(node);
    }

private:
    Node *updatePrevLink(Node *prev, Node *node)
    {
        Node *last = nullptr;
        Node *next2;

        for (;;)
        {
            /* reference the `next` link of previous node */
            Node *prev2 = ref(prev->next);

            /* `prev` link was removed */
            if (!prev2)
            {
                if (!last)
                {
                    prev2 = ref(prev->prev);
                    unref(prev);
                    prev = prev2;
                }
                else
                {
                    markDelete(prev);
                    next2 = ref(prev->next);

                    /* CAS to update `next` link of previous node */
                    if (casLink(last->next, Link(prev), Link(next2)))
                        unref(prev);
                    else
                        unref(next2);

                    /* clear the node */
                    unref(prev);
                    prev = last;
                    last = nullptr;
                }

                continue;
            }

            /* get the `prev` link */
            Link link = node->prev.load();

            /* `prev` link removed */
            if (link.del())
            {
                unref(prev2);
                break;
            }

            /* `next` node of previous node changed, start over */
            if (prev2 != node)
            {
                unref(last);
                last = prev;
                prev = prev2;
                continue;
            }

            /* clear the reference */
            unref(prev2);

            /* check for update done */
            if (link.ptr() == prev2)
                break;

            /* `next` link of previous node is current node, CAS to set the previous link */
            if ((prev->next.load().ptr() == node) && casLink(node->prev, link, Link(prev)))
            {
                ref(prev);
                unref(link.ptr());

                /* `prev` link of previous link is still alive, CAS successful */
                if (!prev->prev.load().del())
                    break;
            }
        }

        /* clear the reference */
        unref(last);
        return prev;
    }

public:
    Node *push_front(const T &value)
    {
        Node *prev = ref(_head);
        Node *next = ref(prev->next);
        Node *node = new Node(value);

        for (;;)
        {
            /* check for deleted link */
            if (prev->next.load() != Link(next))
            {
                unref(next);
                next = ref(prev->next);
                continue;
            }

            /* set previous and next link */
            node->prev = Link(prev);
            node->next = Link(next);

            /* perform CAS operation */
            if (casLink(prev->next, Link(next), Link(node)))
            {
                ref(node);
                break;
            }
        }

        /* update counter, and set the `prev` link */
        _count++;
        pushComplete(node, next);
        return node;
    }
};
}

#endif /* REDSCRIPT_LOCKFREE_DOUBLYLINKEDLIST_H */
