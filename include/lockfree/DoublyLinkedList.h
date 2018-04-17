#ifndef REDSCRIPT_LOCKFREE_DOUBLYLINKEDLIST_H
#define REDSCRIPT_LOCKFREE_DOUBLYLINKEDLIST_H

#include <new>
#include <cstdint>
#include <cstdlib>

#include "lockfree/HazardMemory.h"

namespace RedScript::LockFree
{
template <typename T>
class DoublyLinkedList
{
    struct Node;
    struct Link;

public:
    typedef T value_type;

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
        T data;
        Link prev;
        Link next;

    public:
        template <typename ... Args>
        Node(Args &&... args) : data(std::forward<Args>(args) ...) {}

    };

private:
    struct Limits : public HazardLimits
    {
        static constexpr size_t MAX_ITER    = 2;
        static constexpr size_t MAX_LINKS   = 2;
        static constexpr size_t MAX_DELETES = 2;
        static constexpr size_t MAX_HAZARDS = MAX_ITER + 5;
        static constexpr size_t MAX_THREADS = HazardLimits::MAX_THREADS;
    };

private:
    struct Memory : public HazardMemory<Node, Link, Limits>
    {
        void nodeUpdate(Node *node) override
        {
            Node *prev, *prev2;
            Node *next, *next2;

            /* process the `prev` side */
            while ((prev = Memory::retain(node->prev)))
            {
                /* already deleted */
                if (!prev->prev.del())
                {
                    Memory::release(prev);
                    break;
                }

                /* move to it's predecessor */
                prev2 = Memory::retain(prev->prev);
                node->prev.referenceCAS(Link(prev, true), Link(prev2, true));

                /* release references */
                Memory::release(prev);
                Memory::release(prev2);
            }

            /* process the `next` side */
            while ((next = Memory::retain(node->next)))
            {
                /* already deleted */
                if (!next->next.del())
                {
                    Memory::release(next);
                    break;
                }

                /* move to it's predecessor */
                next2 = Memory::retain(next->next);
                node->next.referenceCAS(Link(next, true), Link(next2, true));

                /* release references */
                Memory::release(next);
                Memory::release(next2);
            }
        }

    public:
        void nodeTerminate(Node *node, bool isConcurrent) override
        {
            if (!isConcurrent)
            {
                node->prev.setUnsafe(Link(nullptr, true));
                node->next.setUnsafe(Link(nullptr, true));
            }
            else
            {
                node->prev.referenceCAS(Link(node->prev.ptr()), Link(nullptr, true));
                node->next.referenceCAS(Link(node->next.ptr()), Link(nullptr, true));
            }
        }
    };

private:
    Link _head;
    Link _tail;
    Memory _mem;
    std::atomic_size_t _count;

public:
    template <typename U>
    class IteratorImpl
    {
        Node *_pos = nullptr;
        DoublyLinkedList *_list = nullptr;

    private:
        friend class DoublyLinkedList;

    public:
        typedef U *                             pointer;
        typedef U &                             reference;
        typedef U                               value_type;
        typedef size_t                          difference_type;
        typedef std::bidirectional_iterator_tag iterator_category;

    public:
       ~IteratorImpl() { if (_pos) _list->_mem.release(_pos); }
        IteratorImpl(const DoublyLinkedList *list, bool isEnd) : IteratorImpl(list, (isEnd ? list->_tail : list->_head).ptr()) {}
        IteratorImpl(const DoublyLinkedList *list, Node *node) : _list(const_cast<DoublyLinkedList *>(list)) { _pos = _list->_mem.retain(node); }

    public:
        IteratorImpl() = default;
        IteratorImpl(IteratorImpl &&other) { swap(other); }
        IteratorImpl(const IteratorImpl &other) { assign(other); }

    public:
        IteratorImpl &operator=(IteratorImpl &&other) { swap(other); return *this; }
        IteratorImpl &operator=(const IteratorImpl &other) { assign(other); return *this; }

    public:
        void swap(IteratorImpl &other)
        {
            std::swap(this->_pos, other._pos);
            std::swap(this->_list, other._list);
        }

    public:
        void assign(const IteratorImpl &other)
        {
            /* reference new node first */
            if (other._pos)
                other._list->_mem.retain(other._pos);

            /* then release the previous reference */
            if (this->_pos)
                this->_list->_mem.release(this->_pos);

            /* replace the current references */
            this->_pos = other._pos;
            this->_list = other._list;
        }

    public:
        IteratorImpl &operator++(void)
        {
            bool dflag = true;
            Node *next;

            /* search until found or meet tail node */
            while (dflag && (_pos != _list->_tail.ptr()))
            {
                next = _list->_mem.retain(_pos->next);
                dflag = next->next.del();

                /* already deleted */
                if (dflag && (_pos->next != Link(next, true)))
                {
                    _list->markDelete(next->prev);
                    _pos->next.referenceCAS(next, next->next.ptr());
                    _list->_mem.release(next);
                    continue;
                }

                /* move to next node */
                _list->_mem.release(_pos);
                _pos = next;
            }

            return *this;
        }

    public:
        IteratorImpl &operator--(void)
        {
            /* search until meets list head */
            while (_pos != _list->_head.ptr())
            {
                /* previous node */
                Node *prev = _list->_mem.retain(_pos->prev);

                /* found one */
                if (!(_pos->next.del()) && (prev->next == Link(_pos)))
                {
                    _list->_mem.release(_pos);
                    _pos = prev;
                    break;
                }

                /* the next node was deleted */
                else if (_pos->next.del())
                {
                    _list->_mem.release(prev);
                    ++(*this);
                }

                /* previous node was modified, update it */
                else
                {
                    /* update previous link */
                    _list->_mem.release(_list->updatePrevLink(prev, _pos));
                }
            }

            return *this;
        }

    public:
        IteratorImpl operator++(int) { auto it = *this; ++(*this); return it; }
        IteratorImpl operator--(int) { auto it = *this; --(*this); return it; }

    public:
        bool operator==(const IteratorImpl& other) const { return _pos == other._pos; }
        bool operator!=(const IteratorImpl& other) const { return _pos != other._pos; }

    public:
        U &operator*()  const { return _pos->data; }
        U *operator->() const { return &(_pos->data); }

    public:
        bool isValid() const { return !(_pos->next.del()); }

    };

public:
    template <typename U>
    class ReverseIteratorImpl
    {
        IteratorImpl<U> _it;

    public:
        typedef U *                             pointer;
        typedef U &                             reference;
        typedef U                               value_type;
        typedef size_t                          difference_type;
        typedef std::bidirectional_iterator_tag iterator_category;

    public:
        ReverseIteratorImpl() = default;
        ReverseIteratorImpl(IteratorImpl<U> &&it) : _it(std::move(it)) {}

    public:
        ReverseIteratorImpl(ReverseIteratorImpl &&other) { swap(other); }
        ReverseIteratorImpl(const ReverseIteratorImpl &other) { assign(other); }

    public:
        ReverseIteratorImpl &operator=(ReverseIteratorImpl &&other) { swap(other); return *this; }
        ReverseIteratorImpl &operator=(const ReverseIteratorImpl &other) { assign(other); return *this; }

    public:
        void swap(ReverseIteratorImpl &other) { std::swap(_it, other._it); }
        void assign(const ReverseIteratorImpl &other) { _it = other._it; }

    public:
        ReverseIteratorImpl &operator++(void) { _it--; return *this; }
        ReverseIteratorImpl &operator--(void) { _it++; return *this; }

    public:
        ReverseIteratorImpl operator++(int) { auto it = *this; _it--; return it; }
        ReverseIteratorImpl operator--(int) { auto it = *this; _it++; return it; }

    public:
        bool operator==(const ReverseIteratorImpl& other) const { return _it == other._it; }
        bool operator!=(const ReverseIteratorImpl& other) const { return _it != other._it; }

    public:
        U &operator*() const { return _it.operator*(); }
        U *operator->() const { return _it.operator->(); }

    public:
        bool isValid() const { return _it.isValid(); }

    };

public:
    typedef IteratorImpl<T>         iterator;
    typedef IteratorImpl<const T>   const_iterator;

public:
    typedef ReverseIteratorImpl<T>          reverse_iterator;
    typedef ReverseIteratorImpl<const T>    const_reverse_iterator;

private:
    void markDelete(Link &link)
    {
        while (true)
        {
            Link &old = link;
            if (old.del() || link.directCAS(old, Link(old.ptr(), true))) break;
        }
    }

private:
    void pushComplete(Node *node, Node *next)
    {
        while (true)
        {
            /* previous node */
            Link &link = next->prev;

            /* check whether it's deleted or moved */
            if (link.del() || (node->next != Link(next)))
                break;

            /* update `prev` pointer */
            if (next->prev.referenceCAS(link, node))
            {
                /* if previous node was deleted, update to the right node */
                if (node->prev.del())
                    node = updatePrevLink(node, next);

                break;
            }
        }

        /* release their references */
        _mem.release(next);
        _mem.release(node);
    }

private:
    Node *updatePrevLink(Node *prev, Node *node)
    {
        bool del;
        Node *last = nullptr;
        Node *prev2;

        while (true)
        {
            Link &link = node->prev;

            /* the previous node was deleted */
            if (link.del())
            {
                if (last)
                {
                    _mem.release(prev);
                    prev = last;
                    last = nullptr;
                }

                break;
            }

            /* state of the next of the previous node */
            del = prev->next.del();
            prev2 = _mem.retain(prev->next);

            /* also deleted */
            if (del)
            {
                if (!last)
                {
                    _mem.release(prev2);
                    prev2 = _mem.retain(prev->prev);
                    _mem.release(prev);
                    prev = prev2;
                }
                else
                {
                    /* update to a maybe valid node */
                    markDelete(prev->prev);
                    last->next.referenceCAS(prev, prev2);

                    /* release their references */
                    _mem.release(prev);
                    _mem.release(prev2);

                    /* and move to the maybe corrent node */
                    prev = last;
                    last = nullptr;
                }

                continue;
            }

            /* some nodes inserted before the correction is done */
            if (prev2 != node)
            {
                /* release the last node */
                if (last)
                    _mem.release(last);

                /* and move to the maybe corrent node */
                last = prev;
                prev = prev2;
                continue;
            }

            /* release the reference */
            _mem.release(prev2);

            /* update `prev` link using CAS */
            if (node->prev.referenceCAS(link, prev))
            {
                if (!prev->prev.del())
                    break;
                else
                    continue;
            }
        }

        /* release reference of the node */
        if (last)
            _mem.release(last);

        /* the corrent `prev` node */
        return prev;
    }

public:
    DoublyLinkedList() : _count(0)
    {
        /* initialize list head and tail */
        _head.setUnsafe(_mem.alloc());
        _tail.setUnsafe(_mem.alloc());

        /* link as circular doubly linked list */
        _head.ptr()->next.setUnsafe(_tail);
        _tail.ptr()->prev.setUnsafe(_head);

        /* avoid cyclic reference */
        _mem.release(_head.ptr());
        _mem.release(_tail.ptr());
    }

public:
    ~DoublyLinkedList()
    {
        clear();
        _mem.free(_head.ptr());
        _mem.free(_tail.ptr());
    }

public:
    bool empty(void) const { return size() == 0; }
    size_t size(void) const { return _count.load(); }

public:
    iterator end(void)   { return   iterator(this,  true); }
    iterator begin(void) { return ++iterator(this, false); }

public:
    iterator end(void)   const { return   iterator(this,  true); }
    iterator begin(void) const { return ++iterator(this, false); }

public:
    reverse_iterator rend(void)   { return reverse_iterator(  iterator(this, false)); }
    reverse_iterator rbegin(void) { return reverse_iterator(--iterator(this,  true)); }

public:
    reverse_iterator rend(void)   const { return reverse_iterator(  iterator(this, false)); }
    reverse_iterator rbegin(void) const { return reverse_iterator(--iterator(this,  true)); }

public:
    bool back(T &value) const
    {
        /* last item */
        auto it = rbegin();

        /* validate the item */
        if ((it == rend()) || !it.isValid())
            return false;

        /* copy to value */
        value = *it;
        return true;
    }

public:
    bool front(T &value) const
    {
        /* first item */
        auto it = begin();

        /* validate the item */
        if ((it == end()) || !it.isValid())
            return false;

        /* copy to value */
        value = *it;
        return true;
    }

public:
    void clear(void)
    {
        /* simply erase every element */
        if (_count.load())
            for (iterator it = begin(); it != end(); erase(it));
    }

public:
    bool erase(iterator &it, T *value = nullptr)
    {
        Node *node = it._pos;
        Node *prev = nullptr;

        /* check for iterator node */
        if ((node == _head.ptr()) || (node == _tail.ptr()))
            throw std::invalid_argument("List head and tail are not erasable");

        while (true)
        {
            bool del = it._pos->next.del();
            Node *next = _mem.retain(it._pos->next);

            /* already erased before */
            if (del)
            {
                _mem.release(next);
                break;
            }

            /* CAS with next node to set it's `del` flag */
            if (node->next.directCAS(next, Link(next, true)))
            {
                while (true)
                {
                    del = node->prev.del();
                    prev = _mem.retain(node->prev);

                    /* if not deleted, CAS to set the `del` flag */
                    if (del || node->prev.directCAS(prev, Link(prev, true)))
                        break;

                    /* release it's reference */
                    _mem.release(prev);
                }

                /* update previous node */
                _mem.release(updatePrevLink(prev, next));
                _mem.release(next);

                /* extract value if needed */
                if (value)
                    *value = std::move(node->data);

                /* advance to next iterator, and update list counter */
                it++;
                _count--;
                _mem.free(node);
                return true;
            }

            /* release it's reference */
            _mem.release(next);
        }

        /* advance to next iterator */
        it++;
        return false;
    }

public:
    template <typename U>
    iterator insert(const iterator &it, U &&value)
    {
        /* insert at front, just `push_front` */
        if (it._pos == _head.ptr())
        {
            push_front(std::forward<U>(value));
            return begin();
        }

        /* create a new node */
        auto  iter = it;
        Node *next = nullptr;
        Node *prev = _mem.retain(iter._pos->prev);
        Node *node = _mem.alloc(std::forward<U>(value));

        while (true)
        {
            /* search for next available node */
            while (iter._pos->next.del())
            {
                iter++;
                prev = updatePrevLink(prev, iter._pos);
            }

            /* set `prev` and `next` link */
            next = iter._pos;
            node->prev.setUnsafe(prev);
            node->next.setUnsafe(next);

            /* CAS with previous node */
            if (prev->next.referenceCAS(iter._pos, node))
                break;

            /* something changed during inserting, find the correct previous node */
            prev = updatePrevLink(prev, iter._pos);
        }

        /* `updatePrevLink` takes control of our node ref, so add another ref then release the node returned */
        _mem.retain(node);
        _mem.release(prev);
        _mem.release(updatePrevLink(node, next));
        _mem.release(next);

        /* update iterator position */
        _count++;
        iter._pos = node;
        return iter;
    }

public:
    bool pop_back(T *value = nullptr)
    {
        Node *next = _mem.retain(_tail);
        Node *node = _mem.retain(next->prev);

        while (true)
        {
            /* something changed during pop */
            if (node->next != Link(next))
            {
                node = updatePrevLink(node, next);
                continue;
            }

            /* meets list head, nothing to pop */
            if (node == _head.ptr())
            {
                _mem.release(node);
                _mem.release(next);
                return false;
            }

            /* CAS with list tail */
            if (node->next.referenceCAS(next, Link(next, true)))
            {
                /* update previous node */
                _mem.release(updatePrevLink(_mem.retain(node->prev), next));
                _mem.release(next);

                /* extract value if needed */
                if (value)
                    *value = std::move(node->data);

                /* release the node */
                _mem.release(node);
                _mem.free(node);
                break;
            }
        }

        /* update list counter */
        _count--;
        return true;
    }

public:
    bool pop_front(T *value = nullptr)
    {
        /* reference the list head */
        Node *node;
        Node *prev = _mem.retain(_head);

        while (true)
        {
            /* meets list tail, nothing to pop */
            if ((node = _mem.retain(prev->next)) == _tail.ptr())
            {
                _mem.release(node);
                _mem.release(prev);
                return false;
            }

            /* suppose to be the second node of the list */
            bool del = node->next.del();
            Node *next = _mem.retain(node->next);

            /* node was deleted */
            if (del)
            {
                markDelete(node->prev);
                prev->next.referenceCAS(node, next);
                _mem.release(next);
                _mem.release(node);
                continue;
            }

            /* CAS with list head */
            if (node->next.referenceCAS(next, Link(next, true)))
            {
                /* update previous node */
                _mem.release(updatePrevLink(prev, next));
                _mem.release(next);

                /* extract value if needed */
                if (value)
                    *value = std::move(node->data);

                /* release the node */
                _mem.release(node);
                _mem.free(node);
                break;
            }

            /* release their references */
            _mem.release(next);
            _mem.release(node);
        }

        /* update node counter */
        _count--;
        return true;
    }

public:
    template <typename U>
    void push_back(U &&value, iterator *it = nullptr)
    {
        Node *next = _mem.retain(_tail);
        Node *prev = _mem.retain(next->prev);
        Node *node = _mem.alloc(std::forward<U>(value));

        while (true)
        {
            /* set `prev` and `next` pointer */
            node->prev.setUnsafe(prev);
            node->next.setUnsafe(next);

            /* create iterator if needed */
            if (it)
            {
                /* release old node if any */
                if (it->_pos)
                    it->_list->_mem.release(it->_pos);

                /* update iterator */
                it->_pos = _mem.retain(node);
                it->_list = this;
            }

            /* CAS with list head */
            if (prev->next.referenceCAS(next, node))
                break;

            /* find the proper `prev` link */
            prev = updatePrevLink(prev, next);
        }

        /* update list counter */
        _count++;
        _mem.release(prev);
        pushComplete(node, next);
    }

public:
    template <typename U>
    void push_front(U &&value, iterator *it = nullptr)
    {
        Node *prev = _mem.retain(_head);
        Node *next = _mem.retain(prev->next);
        Node *node = _mem.alloc(std::forward<U>(value));

        while (true)
        {
            /* set `prev` and `next` pointer */
            node->prev.setUnsafe(prev);
            node->next.setUnsafe(next);

            /* create iterator if needed */
            if (it)
            {
                /* release old node if any */
                if (it->_pos)
                    it->_list->_mem.release(it->_pos);

                /* update iterator */
                it->_pos = _mem.retain(node);
                it->_list = this;
            }

            /* CAS with list head */
            if (prev->next.referenceCAS(next, node))
                break;

            /* prepare for next try */
            _mem.release(next);
            next = _mem.retain(prev->next);
        }

        /* update list counter */
        _count++;
        _mem.release(prev);
        pushComplete(node, next);
    }
};
}

#endif /* REDSCRIPT_LOCKFREE_DOUBLYLINKEDLIST_H */
