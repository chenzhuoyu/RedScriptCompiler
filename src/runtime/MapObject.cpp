#include "runtime/MapObject.h"

namespace RedScript::Runtime
{
/* type object for map */
TypeRef MapTypeObject;

Runtime::ObjectRef MapObject::back(void)
{
    Utils::RWLock::Read _(_rwlock);
    return _head.prev->value;
}

Runtime::ObjectRef MapObject::front(void)
{
    Utils::RWLock::Read _(_rwlock);
    return _head.next->value;
}

Runtime::ObjectRef MapObject::pop(Runtime::ObjectRef key)
{
    Node *node;
    {
        /* search for the key, constrain the lock within a scope */
        Utils::RWLock::Write _(_rwlock);
        auto it = _map.find(key);

        /* check for existance */
        if (it == _map.end())
            return nullptr;

        /* erase from map */
        node = it->second;
        _map.erase(it);

        /* detach from node list if ordered or LRU */
        if (_mode != Mode::Unordered)
            detach(node);
    }

    /* extract the value, move to prevent copy */
    Runtime::ObjectRef value = std::move(node->value);

    /* clear node instance, outside of lock */
    delete node;
    return std::move(value);
}

Runtime::ObjectRef MapObject::find(Runtime::ObjectRef key)
{
    /* search for the key */
    Utils::RWLock::Read _(_rwlock);
    auto it = _map.find(key);

    /* check for existance */
    if (it == _map.end())
        return nullptr;

    /* move the node to head if LRU */
    if (_mode == Mode::LRU)
    {
        detach(it->second);
        attach(it->second, &_head);
    }

    /* read the node value */
    return it->second->value;
}

bool MapObject::has(Runtime::ObjectRef key)
{
    Utils::RWLock::Read _(_rwlock);
    return _map.find(key) != _map.end();
}

void MapObject::insert(Runtime::ObjectRef key, Runtime::ObjectRef value)
{
    /* search for the key */
    Utils::RWLock::Write _(_rwlock);
    auto it = _map.find(key);
    Node *node;

    /* check for existance */
    if (it == _map.end())
    {
        /* node not exists, create new node */
        node = new Node(key, value);
        _map.emplace(key, node);

        /* attach to node list, if ordered or LRU */
        if (_mode != Mode::Unordered)
            attach(node, &_head);
    }
    else
    {
        /* already exists, replace it's value */
        node = it->second;
        node->value = std::move(value);

        /* move to queue head if LRU */
        if (_mode == Mode::LRU)
        {
            detach(node);
            attach(node, &_head);
        }
    }
}

void MapObject::initialize(void)
{
    /* map type object */
    static MapType nullType;
    MapTypeObject = Reference<MapType>::refStatic(nullType);
}
}
