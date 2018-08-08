
#include <runtime/MapObject.h>

#include "runtime/MapObject.h"

namespace RedScript::Runtime
{
/* type object for map */
TypeRef MapTypeObject;

size_t MapObject::size(void)
{
    Utils::RWLock::Read _(_lock);
    return _map.size();
}

Runtime::ObjectRef MapObject::firstKey(void)
{
    Utils::RWLock::Read _(_lock);
    return _head.next->key;
}

Runtime::ObjectRef MapObject::firstValue(void)
{
    Utils::RWLock::Read _(_lock);
    return _head.next->value;
}

Runtime::ObjectRef MapObject::pop(Runtime::ObjectRef key)
{
    Node *node;
    {
        /* search for the key, constrain the lock within a scope */
        Utils::RWLock::Write _(_lock);
        auto it = _map.find(key);

        /* check for existance */
        if (it == _map.end())
            return nullptr;

        /* erase from map, and detach from node list */
        node = it->second;
        _map.erase(it);
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
    /* non-LRU map, find operation is read-only */
    if (_mode != Mode::LRU)
    {
        Utils::RWLock::Read _(_lock);
        auto it = _map.find(key);

        /* check for existance */
        if (it == _map.end())
            return nullptr;

        /* read the node value */
        return it->second->value;
    }

    /* LRU-map may alter the traverse order */
    else
    {
        Utils::RWLock::Write _(_lock);
        auto it = _map.find(key);

        /* check for existance */
        if (it == _map.end())
            return nullptr;

        /* move the node to head */
        detach(it->second);
        attach(it->second, &_head);

        /* read the node value */
        return it->second->value;
    }
}

bool MapObject::has(Runtime::ObjectRef key)
{
    Utils::RWLock::Read _(_lock);
    return _map.find(key) != _map.end();
}

void MapObject::insert(Runtime::ObjectRef key, Runtime::ObjectRef value)
{
    /* search for the key */
    Utils::RWLock::Write _(_lock);
    auto it = _map.find(key);
    Node *node;

    /* check for existance */
    if (it == _map.end())
    {
        /* node not exists, create new node and attach to node list */
        node = new Node(key, value);
        _map.emplace(key, node);
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
    Utils::RWLock::Write _(_lock);

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
    Utils::RWLock::Read _(_lock);

    /* traverse the list, which preserve the order */
    for (Node *node = _head.next; node != &_head; node = node->next)
        if (!(func(node->key, node->value)))
            break;
}

void MapObject::enumerateCopy(MapObject::EnumeratorFunc func)
{
    std::vector<Runtime::ObjectRef> keys;
    std::vector<Runtime::ObjectRef> values;

    {
        /* restrict the lock within scope, and perform
         * copy operations before actual enumeration */
        Utils::RWLock::Read _(_lock);

        /* reserve space for key value pair */
        keys.reserve(_map.size());
        values.reserve(_map.size());

        /* traverse the list, which preserve the order */
        for (Node *node = _head.next; node != &_head; node = node->next)
        {
            keys.emplace_back(node->key);
            values.emplace_back(node->value);
        }
    }

    /* now left the locking zone, invoke the callback */
    for (size_t i = 0; i < keys.size(); i++)
        if (!(func(std::move(keys[i]), std::move(values[i]))))
            break;
}

void MapObject::shutdown(void)
{
    /* clear type instance */
    MapTypeObject = nullptr;
}

void MapObject::initialize(void)
{
    /* map type object */
    static MapType nullType;
    MapTypeObject = Reference<MapType>::refStatic(nullType);
}
}
