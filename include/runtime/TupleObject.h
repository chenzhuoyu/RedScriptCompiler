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
class TupleType : public NativeType
{
public:
    explicit TupleType() : NativeType("tuple") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

/*** Native Object Protocol ***/

public:
    virtual uint64_t    nativeObjectHash(ObjectRef self) override;
    virtual std::string nativeObjectRepr(ObjectRef self) override;

public:
    virtual bool nativeObjectIsTrue(ObjectRef self) override;

/*** Native Numeric Protocol ***/

public:
    virtual ObjectRef nativeNumericAdd   (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericMul   (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericIncAdd(ObjectRef self, ObjectRef other) override { return nativeNumericAdd(self, other); }
    virtual ObjectRef nativeNumericIncMul(ObjectRef self, ObjectRef other) override { return nativeNumericMul(self, other); }

/*** Native Iterator Protocol ***/

public:
    virtual ObjectRef nativeIterableIter(ObjectRef self) override;

/*** Native Sequence Protocol ***/

public:
    virtual ObjectRef nativeSequenceLen     (ObjectRef self) override;
    virtual ObjectRef nativeSequenceGetItem (ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeSequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step) override;

/*** Native Comparator Protocol ***/

public:
    virtual ObjectRef nativeComparableEq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableLt(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableGt(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef nativeComparableNeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableLeq(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableGeq(ObjectRef self, ObjectRef other) override;

public:
    virtual ObjectRef nativeComparableCompare(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeComparableContains(ObjectRef self, ObjectRef other) override;

};

class TupleIteratorType : public NativeType
{
public:
    explicit TupleIteratorType() : NativeType("tuple_iterator") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

/*** Native Iterator Protocol ***/

public:
    virtual ObjectRef nativeIterableNext(ObjectRef self) override;

};

/* type object for tuple and tuple iterator */
extern TypeRef TupleTypeObject;
extern TypeRef TupleIteratorTypeObject;

class TupleObject : public Object
{
    size_t _size;
    ObjectRef *_items;
    friend class TupleType;
    friend class TupleIteratorObject;

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
        if (_pos < _tuple->_size)
            return _tuple->_items[_pos++];

        /* clear the reference before throwing the exception
         * to prevent cyclic reference to the tuple object */
        _tuple = nullptr;
        throw Exceptions::StopIteration();
    }
};
}

#endif /* REDSCRIPT_RUNTIME_TUPLEOBJECT_H */
