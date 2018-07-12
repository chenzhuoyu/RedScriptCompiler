#ifndef REDSCRIPT_RUNTIME_TUPLEOBJECT_H
#define REDSCRIPT_RUNTIME_TUPLEOBJECT_H

#include <string>
#include <cstdint>
#include <initializer_list>

#include "utils/Lists.h"
#include "runtime/Object.h"
#include "exceptions/StopIteration.h"

namespace RedScript::Runtime
{
class TupleType : public Type
{
public:
    explicit TupleType() : Type("tuple") {}

/*** Object Protocol ***/

public:
    virtual uint64_t    objectHash(ObjectRef self) override;
    virtual std::string objectRepr(ObjectRef self) override;

public:
    virtual bool objectIsTrue(ObjectRef self) override;

/*** Numeric Protocol ***/

public:
    virtual ObjectRef numericAdd   (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericMul   (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncAdd(ObjectRef self, ObjectRef other) override { return numericAdd(self, other); }
    virtual ObjectRef numericIncMul(ObjectRef self, ObjectRef other) override { return numericMul(self, other); }

/*** Iterator Protocol ***/

public:
    virtual ObjectRef iterableIter(ObjectRef self) override;

/*** Sequence Protocol ***/

public:
    virtual ObjectRef sequenceLen    (ObjectRef self) override;
    virtual ObjectRef sequenceGetItem(ObjectRef self, ObjectRef other) override;

/*** Comparator Protocol ***/

public:
    virtual ObjectRef comparableEq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableLt(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableGt(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef comparableNeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableLeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableGeq(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef comparableCompare(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef comparableContains(ObjectRef self, ObjectRef other) override;

};

class TupleIteratorType : public Type
{
public:
    explicit TupleIteratorType() : Type("tuple_iterator") {}

/*** Iterator Protocol ***/

public:
    virtual ObjectRef iterableNext(ObjectRef self);

};

/* type object for tuple and tuple iterator */
extern TypeRef TupleTypeObject;
extern TypeRef TupleIteratorTypeObject;

class TupleObject : public Object
{
    size_t _size;
    ObjectRef *_items;

public:
    virtual ~TupleObject() { delete[] _items; }
    explicit TupleObject(size_t size);

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
        Utils::Lists::fill(result->_items, std::forward<Args>(items) ...);
        return std::move(result);
    }

public:
    virtual void referenceClear(void) override;
    virtual void referenceTraverse(VisitFunction visit) override;

public:
    static void shutdown(void) {}
    static void initialize(void);

};

class TupleIteratorObject : public Object
{
    size_t _pos;
    Reference<TupleObject> _tuple;

public:
    virtual ~TupleIteratorObject() = default;
    explicit TupleIteratorObject(Reference<TupleObject> tuple) : Object(TupleIteratorTypeObject), _pos(0), _tuple(tuple) {}

public:
    ObjectRef next(void)
    {
        /* check for tuple */
        if (_tuple.isNull())
            throw Exceptions::StopIteration();

        /* check for iterator position */
        if (_pos < _tuple->size())
            return _tuple->items()[_pos++];

        /* clear the reference before throwing the exception
         * to prevent cyclic reference to the tuple object */
        _tuple = nullptr;
        throw Exceptions::StopIteration();
    }
};
}

#endif /* REDSCRIPT_RUNTIME_TUPLEOBJECT_H */
