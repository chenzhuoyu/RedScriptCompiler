#ifndef REDSCRIPT_UTILS_DOUBLYLINKEDLIST_H
#define REDSCRIPT_UTILS_DOUBLYLINKEDLIST_H

#include <cstdint>

namespace RedScript::Utils
{
struct DoublyLinkedList
{
    struct Node;
    class Tag
    {
        uintptr_t _ptr;

    public:
        Tag(Node *p) : _ptr(reinterpret_cast<uintptr_t>(p)) {}

    public:
        Node *node(void) const { return reinterpret_cast<Node *>(_ptr); }

    };

public:
    struct Node
    {
        Tag prev;
        Tag next;

    public:
        Node() : prev(this), next(this) {}

    };

private:
    Node _list;
    static_assert(sizeof(Node) == 16, "Non lock-free node");

};
}

#endif /* REDSCRIPT_UTILS_DOUBLYLINKEDLIST_H */
