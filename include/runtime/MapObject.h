#ifndef REDSCRIPT_RUNTIME_MAPOBJECT_H
#define REDSCRIPT_RUNTIME_MAPOBJECT_H

#include <string>
#include <cstdint>
#include <functional>
#include <unordered_map>

#include "utils/RWLock.h"
#include "runtime/Object.h"

namespace RedScript::Runtime
{
class MapType : public NativeType
{
public:
    explicit MapType() : NativeType("map") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

};

/* type object for map */
extern TypeRef MapTypeObject;

class MapObject : public Object
{
    struct Node
    {
        Node *prev;
        Node *next;
        Runtime::ObjectRef key;
        Runtime::ObjectRef value;

    public:
        Node() : prev(this), next(this), key(nullptr), value(nullptr) {}
        Node(Runtime::ObjectRef &key, Runtime::ObjectRef &value) : prev(this), next(this), key(key), value(value) {}

    };

public:
    enum class Mode : int
    {
        LRU,
        Ordered,
    };

public:
    typedef std::function<bool(Runtime::ObjectRef, Runtime::ObjectRef)> EnumeratorFunc;

private:
    Mode _mode;
    Node _head;
    Utils::RWLock _lock;
    std::unordered_map<Runtime::ObjectRef, Node *> _map;

private:
    static inline void detach(Node *node)
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

private:
    static inline void attach(Node *node, Node *head)
    {
        node->next = head;
        node->prev = head->prev;
        head->prev->next = node;
        head->prev = node;
    }

public:
    virtual ~MapObject() { clear(); }
    explicit MapObject(Mode mode = Mode::Ordered) : Object(MapTypeObject), _mode(mode) {}

public:
    Mode mode(void) const { return _mode; }
    size_t size(void);

public:
    Runtime::ObjectRef firstKey(void);
    Runtime::ObjectRef firstValue(void);

public:
    Runtime::ObjectRef pop(Runtime::ObjectRef key);
    Runtime::ObjectRef find(Runtime::ObjectRef key);

public:
    bool has(Runtime::ObjectRef key);
    bool remove(Runtime::ObjectRef key) { return bool(pop(key)); }
    void insert(Runtime::ObjectRef key, Runtime::ObjectRef value);

public:
    void clear(void);
    void enumerate(EnumeratorFunc func);
    void enumerateCopy(EnumeratorFunc func);

public:
    static Reference<MapObject> newLRU(void)     { return Object::newObject<MapObject>(Mode::LRU); }
    static Reference<MapObject> newOrdered(void) { return Object::newObject<MapObject>(Mode::Ordered); }

public:
    static void shutdown(void);
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_MAPOBJECT_H */
