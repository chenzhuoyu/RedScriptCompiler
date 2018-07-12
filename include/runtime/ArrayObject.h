#ifndef REDSCRIPT_RUNTIME_ARRAYOBJECT_H
#define REDSCRIPT_RUNTIME_ARRAYOBJECT_H

#include <string>
#include <vector>
#include <cstdint>

#include "utils/Lists.h"
#include "utils/RWLock.h"
#include "runtime/Object.h"
#include "exceptions/RuntimeError.h"
#include "exceptions/StopIteration.h"

namespace RedScript::Runtime
{
class ArrayType : public Type
{
public:
    explicit ArrayType() : Type("array") {}

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
    virtual ObjectRef numericIncAdd(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef numericIncMul(ObjectRef self, ObjectRef other) override;

/*** Iterator Protocol ***/

public:
    virtual ObjectRef iterableIter(ObjectRef self) override;

/*** Sequence Protocol ***/

public:
    virtual ObjectRef sequenceLen    (ObjectRef self) override;
    virtual void      sequenceDelItem(ObjectRef self, ObjectRef other) override;
    virtual ObjectRef sequenceGetItem(ObjectRef self, ObjectRef other) override;
    virtual void      sequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third) override;

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

class ArrayIteratorType : public Type
{
public:
    explicit ArrayIteratorType() : Type("array_iterator") {}

/*** Iterator Protocol ***/

public:
    virtual ObjectRef iterableNext(ObjectRef self) override;

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
    ObjectRef itemAt(size_t index);
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
    static void shutdown(void) {}
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
