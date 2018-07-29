#ifndef REDSCRIPT_RUNTIME_ARRAYOBJECT_H
#define REDSCRIPT_RUNTIME_ARRAYOBJECT_H

#include <string>
#include <vector>
#include <cstdint>

#include "utils/Lists.h"
#include "utils/RWLock.h"

#include "runtime/Object.h"
#include "runtime/ExceptionObject.h"

namespace RedScript::Runtime
{
class ArrayType : public NativeType
{
public:
    explicit ArrayType() : NativeType("array") {}

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
    virtual ObjectRef nativeNumericIncAdd(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeNumericIncMul(ObjectRef self, ObjectRef other) override;

/*** Native Iterator Protocol ***/

public:
    virtual ObjectRef nativeIterableIter(ObjectRef self) override;

/*** Native Sequence Protocol ***/

public:
    virtual ObjectRef nativeSequenceLen    (ObjectRef self) override;
    virtual void      nativeSequenceDelItem(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef nativeSequenceGetItem(ObjectRef self, ObjectRef other) override;
    virtual void      nativeSequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third) override;

public:
    virtual void      nativeSequenceDelSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step) override;
    virtual ObjectRef nativeSequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step) override;
    virtual void      nativeSequenceSetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step, ObjectRef value) override;

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

class ArrayIteratorType : public NativeType
{
public:
    explicit ArrayIteratorType() : NativeType("array_iterator") {}

protected:
    virtual void addBuiltins(void) override {}
    virtual void clearBuiltins(void) override {}

/*** Native Iterator Protocol ***/

public:
    virtual ObjectRef nativeIterableNext(ObjectRef self) override;

};

/* type object for array and array iterator */
extern TypeRef ArrayTypeObject;
extern TypeRef ArrayIteratorTypeObject;

class ArrayObject : public Object
{
    size_t _mods;
    Utils::RWLock _lock;
    std::vector<ObjectRef> _items;
    friend class ArrayIteratorObject;

public:
    virtual ~ArrayObject() = default;
    explicit ArrayObject() : ArrayObject(0) {}
    explicit ArrayObject(size_t size) : Object(ArrayTypeObject), _mods(0), _items(size) { track(); }
    explicit ArrayObject(const std::vector<ObjectRef> &items) : Object(ArrayTypeObject), _mods(0), _items(items) { track(); }

public:
    size_t size(void) const { return _items.size(); }
    std::vector<ObjectRef> &items(void) { return _items; }

public:
    void repeat(size_t count);
    void append(ObjectRef item);
    void concat(Reference<ArrayObject> items);

public:
    void setItemAt(size_t index, ObjectRef value);
    void removeItemAt(size_t index);

public:
    void setSlice(size_t begin, size_t count, ssize_t step, ObjectRef items);
    void removeSlice(size_t begin, size_t count, ssize_t step);

public:
    ObjectRef itemAt(size_t index);
    Reference<ArrayObject> slice(size_t begin, size_t count, ssize_t step);

public:
    Reference<ArrayObject> repeatCopy(size_t count);
    Reference<ArrayObject> concatCopy(Reference<ArrayObject> items);

public:
    static Reference<ArrayObject> newEmpty(void) { return fromSize(0); }
    static Reference<ArrayObject> fromSize(size_t size) { return Object::newObject<ArrayObject>(size); }
    static Reference<ArrayObject> fromArray(const std::vector<ObjectRef> &items) { return Object::newObject<ArrayObject>(items); }

public:
    template <typename ... Args>
    static Reference<ArrayObject> fromObjects(Args && ... items)
    {
        Reference<ArrayObject> result = fromSize(sizeof ... (Args));
        Utils::Lists::fill(result->_items, std::forward<Args>(items) ...);
        return std::move(result);
    }

public:
    virtual void referenceClear(void) override { _items.clear(); }
    virtual void referenceTraverse(VisitFunction visit) override { for (auto &x : _items) if (x) visit(x); }

public:
    static void shutdown(void);
    static void initialize(void);

};

class ArrayIteratorObject : public Object
{
    size_t _pos;
    size_t _mods;
    Reference<ArrayObject> _array;

public:
    virtual ~ArrayIteratorObject() = default;
    explicit ArrayIteratorObject(Reference<ArrayObject> array) :
        Object(ArrayIteratorTypeObject), _pos(0), _mods(array->_mods), _array(array) {}

public:
    ObjectRef next(void)
    {
        /* check for array */
        if (_array.isNull())
            throw Exceptions::StopIteration();

        /* lock before reading */
        Utils::RWLock::Read _(_array->_lock);

        /* check for modification counter */
        if (_mods != _array->_mods)
            throw Exceptions::RuntimeError("Array modified during iteration");

        /* check for iterator position */
        if (_pos < _array->_items.size())
            return _array->_items[_pos++];

        /* clear the reference before throwing the exception
         * to prevent cyclic reference to the array object */
        _array = nullptr;
        throw Exceptions::StopIteration();
    }
};
}

#endif /* REDSCRIPT_RUNTIME_ARRAYOBJECT_H */
