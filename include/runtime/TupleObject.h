#ifndef REDSCRIPT_RUNTIME_TUPLEOBJECT_H
#define REDSCRIPT_RUNTIME_TUPLEOBJECT_H

#include <string>
#include <cstdint>
#include <initializer_list>

#include "runtime/Object.h"

namespace RedScript::Runtime
{
class TupleType : public Type
{
public:
    explicit TupleType() : Type("tuple") {}

};

/* type object for tuples */
extern TypeRef TupleTypeObject;

namespace Details
{
template <size_t, typename T>
static void fillObjects(T *items) {}

template <size_t I, typename T, typename ... Args>
static void fillObjects(T *items, T item, Args && ... remains)
{
    items[I] = item;
    fillObjects<I + 1, T>(items, std::forward<Args>(remains) ...);
}
}

class TupleObject : public Object
{
    size_t _size;
    ObjectRef *_items;

public:
    virtual ~TupleObject() { delete [] _items; }
    explicit TupleObject(size_t size) : Object(TupleTypeObject), _size(size), _items(new ObjectRef[size]) {}

public:
    size_t size(void) const { return _size; }
    ObjectRef *items(void) const { return _items; }

public:
    static Reference<TupleObject> newEmpty(void) { return fromSize(0); }
    static Reference<TupleObject> fromSize(size_t size) { return Object::newObject<TupleObject>(size); }

public:
    template <typename ... Args>
    static Reference<TupleObject> fromObjects(Args && ... items)
    {
        Reference<TupleObject> result = fromSize(sizeof ... (Args));
        Details::fillObjects<0, ObjectRef>(result->_items, std::forward<Args>(items) ...);
        return std::move(result);
    }

public:
    static void shutdown(void) {}
    static void initialize(void);

};
}

#endif /* REDSCRIPT_RUNTIME_TUPLEOBJECT_H */
