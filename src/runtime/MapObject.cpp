#include "runtime/MapObject.h"

namespace RedScript::Runtime
{
/* type object for map */
TypeRef MapTypeObject;

size_t MapObject::size(void)
{
    Utils::RWLock::Read _(_rwlock);
    return _map.size();
}

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

void MapObject::clear(void)
{
    /* lock in exclusive mode */
    Utils::RWLock::Write _(_rwlock);

    /* list head */
    Node *next;
    Node *node = _head.next;

    /* clear each node */
    while (node != &_head)
    {
        next = node->next;
        delete node;
        node = next;
    }

    /* clear map */
    _map.clear();
    _head.prev = &_head;
    _head.next = &_head;
}

void MapObject::enumerate(MapObject::EnumeratorFunc func)
{
    /* lock in shared mode */
    Utils::RWLock::Read _(_rwlock);
    Node *node = _head.next;

    /* traverse each node */
    while ((node != &_head) && func(node->key, node->value))
        node = node->next;
}

void MapObject::enumerateCopy(MapObject::EnumeratorFunc func)
{
    std::vector<Runtime::ObjectRef> keys;
    std::vector<Runtime::ObjectRef> values;

    /* restrict the lock within scope, and perform
     * copy operations before actual enumeration */
    {
        /* lock in shared mode */
        Utils::RWLock::Read _(_rwlock);
        Node *node = _head.next;

        /* reserve space for key value pair */
        keys.reserve(_map.size());
        values.reserve(_map.size());

        /* copy key-value pair into buffer */
        while (node != &_head)
        {
            keys.emplace_back(node->key);
            values.emplace_back(node->value);
            node = node->next;
        }
    }

    /* now left the locking zone, invoke the callback */
    for (size_t i = 0; i < keys.size(); i++)
        if (!func(std::move(keys[i]), std::move(values[i])))
            break;
}

void MapObject::initialize(void)
{
    /* map type object */
    static MapType nullType;
    MapTypeObject = Reference<MapType>::refStatic(nullType);
}
}
