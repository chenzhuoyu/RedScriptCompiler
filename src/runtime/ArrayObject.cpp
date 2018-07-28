#include <algorithm>

#include "utils/Integer.h"
#include "utils/Strings.h"

#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/TupleObject.h"

#include "exceptions/TypeError.h"
#include "exceptions/IndexError.h"
#include "exceptions/ValueError.h"
#include "exceptions/InternalError.h"
#include "exceptions/StopIteration.h"

namespace RedScript::Runtime
{
/* type object for array and array iterator */
TypeRef ArrayTypeObject;
TypeRef ArrayIteratorTypeObject;

/*** Native Object Protocol ***/

uint64_t ArrayType::nativeObjectHash(ObjectRef self)
{
    /* arrays are not hashable */
    throw Exceptions::TypeError("Unhashable type \"array\"");
}

std::string ArrayType::nativeObjectRepr(ObjectRef self)
{
    /* scope control */
    Object::Repr repr(self);
    Reference<ArrayObject> array = self.as<ArrayObject>();
    std::vector<std::string> items;

    /* recursion detected */
    if (repr.isExists())
        return "[...]";

    /* hash each item */
    for (size_t i = 0; i < array->size(); i++)
    {
        auto item = array->items()[i];
        items.emplace_back(item->type()->objectRepr(item));
    }

    /* join them together */
    return Utils::Strings::format("[%s]", Utils::Strings::join(items, ", "));
}

bool ArrayType::nativeObjectIsTrue(ObjectRef self)
{
    /* non-empty arrays represents true */
    return self.as<ArrayObject>()->size() != 0;
}

/*** Numeric Protocol ***/

ObjectRef ArrayType::nativeNumericAdd(ObjectRef self, ObjectRef other)
{
    /* type check */
    if (other->isNotInstanceOf(ArrayTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Can only concatenate array (not \"%s\") to array",
            other->type()->name()
        ));
    }

    /* copy and concat */
    return self.as<ArrayObject>()->concatCopy(other.as<ArrayObject>());
}

ObjectRef ArrayType::nativeNumericMul(ObjectRef self, ObjectRef other)
{
    /* type check */
    if (other->isNotInstanceOf(IntTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Repeating count must be an integer, not \"%s\"",
            other->type()->name()
        ));
    }

    /* convert to correct type */
    Reference<IntObject> val = other.as<IntObject>();
    Reference<ArrayObject> array = self.as<ArrayObject>();

    /* must be an unsigned integer */
    if (!(val->isSafeUInt()))
        throw Exceptions::ValueError("Repeating count must be an unsigned integer");

    /* copy and repeat */
    return array->repeatCopy(val->toUInt());
}

ObjectRef ArrayType::nativeNumericIncAdd(ObjectRef self, ObjectRef other)
{
    /* type check */
    if (other->isNotInstanceOf(ArrayTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Can only concatenate array (not \"%s\") to array",
            other->type()->name()
        ));
    }

    /* concat inplace */
    self.as<ArrayObject>()->concat(other.as<ArrayObject>());
    return self;
}

ObjectRef ArrayType::nativeNumericIncMul(ObjectRef self, ObjectRef other)
{
    /* type check */
    if (other->isNotInstanceOf(IntTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Repeating count must be an integer, not \"%s\"",
            other->type()->name()
        ));
    }

    /* convert to correct type */
    Reference<IntObject> val = other.as<IntObject>();
    Reference<ArrayObject> array = self.as<ArrayObject>();

    /* must be an unsigned integer */
    if (!(val->isSafeUInt()))
        throw Exceptions::ValueError("Repeating count must be an unsigned integer");

    /* repeat inplace */
    array->repeat(val->toUInt());
    return self;
}

/*** Native Iterator Protocol ***/

ObjectRef ArrayType::nativeIterableIter(ObjectRef self)
{
    /* create an iterator from array */
    return Object::newObject<ArrayIteratorObject>(self.as<ArrayObject>());
}

ObjectRef ArrayIteratorType::nativeIterableNext(ObjectRef self)
{
    /* get the next object */
    return self.as<ArrayIteratorObject>()->next();
}

/*** Native Sequence Protocol ***/

ObjectRef ArrayType::nativeSequenceLen(ObjectRef self)
{
    /* get the length, and wrap with integer */
    return IntObject::fromUInt(self.as<ArrayObject>()->size());
}

void ArrayType::nativeSequenceDelItem(ObjectRef self, ObjectRef other)
{
    /* remove item from array */
    auto array = self.as<ArrayObject>();
    array->removeItemAt(Utils::Lists::indexConstraint(array, other));
}

ObjectRef ArrayType::nativeSequenceGetItem(ObjectRef self, ObjectRef other)
{
    /* get item from array */
    auto array = self.as<ArrayObject>();
    return array->itemAt(Utils::Lists::indexConstraint(array, other));
}

void ArrayType::nativeSequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third)
{
    /* set item in array */
    auto array = self.as<ArrayObject>();
    array->setItemAt(Utils::Lists::indexConstraint(array, second), std::move(third));
}

void ArrayType::nativeSequenceDelSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)
{
    auto array = self.as<ArrayObject>();
    auto slice = Utils::Lists::sliceConstraint(array, begin, end, step);
    array->removeSlice(slice.begin, slice.count, slice.step);
}

ObjectRef ArrayType::nativeSequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)
{
    auto array = self.as<ArrayObject>();
    auto slice = Utils::Lists::sliceConstraint(array, begin, end, step);
    return array->slice(slice.begin, slice.count, slice.step);
}

void ArrayType::nativeSequenceSetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step, ObjectRef value)
{
    auto array = self.as<ArrayObject>();
    auto slice = Utils::Lists::sliceConstraint(array, begin, end, step);
    array->setSlice(slice.begin, slice.count, slice.step, std::move(value));
}

/*** Native Comparator Protocol ***/

#define BOOL_CMP(op) {                                              \
    /* object type checking */                                      \
    if (other->isNotInstanceOf(ArrayTypeObject))                    \
        return FalseObject;                                         \
                                                                    \
    /* perform comparison */                                        \
    return BoolObject::fromBool(Utils::Lists::totalOrderCompare(    \
        self.as<ArrayObject>(),                                     \
        other.as<ArrayObject>()                                     \
    ) op 0);                                                        \
}

ObjectRef ArrayType::nativeComparableEq (ObjectRef self, ObjectRef other) BOOL_CMP(==)
ObjectRef ArrayType::nativeComparableLt (ObjectRef self, ObjectRef other) BOOL_CMP(< )
ObjectRef ArrayType::nativeComparableGt (ObjectRef self, ObjectRef other) BOOL_CMP(> )
ObjectRef ArrayType::nativeComparableNeq(ObjectRef self, ObjectRef other) BOOL_CMP(!=)
ObjectRef ArrayType::nativeComparableLeq(ObjectRef self, ObjectRef other) BOOL_CMP(<=)
ObjectRef ArrayType::nativeComparableGeq(ObjectRef self, ObjectRef other) BOOL_CMP(>=)

#undef BOOL_CMP

ObjectRef ArrayType::nativeComparableCompare(ObjectRef self, ObjectRef other)
{
    /* object type checking */
    if (other->isNotInstanceOf(ArrayTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"array\" and \"%s\" objects are not comparable",
            other->type()->name()
        ));
    }

    /* call the total-order comparing function */
    return IntObject::fromInt(Utils::Lists::totalOrderCompare(self.as<ArrayObject>(), other.as<ArrayObject>()));
}

ObjectRef ArrayType::nativeComparableContains(ObjectRef self, ObjectRef other)
{
    Reference<ArrayObject> array = self.as<ArrayObject>();
    return BoolObject::fromBool(Utils::Lists::contains(std::move(array), std::move(other)));
}

void ArrayObject::repeat(size_t count)
{
    /* lock before modifying, and update the modification counter */
    Utils::RWLock::Write _(_lock);
    _mods++;

    /* shortcut for times == 0 and 1 */
    if (count == 0)
    {
        _items.clear();
        return;
    }

    /* reserve space for better performance */
    if (count > 1)
        _items.reserve(_items.size() * count);

    /* fill the remaining items */
    for (size_t i = 0; i < count - 1; i++)
    {
        _items.insert(
            std::end(_items),
            std::begin(_items),
            std::begin(_items) + count
        );
    }
}

void ArrayObject::append(ObjectRef item)
{
    /* lock before modifying */
    Utils::RWLock::Write _(_lock);

    /* update the modification counter and item list */
    _mods++;
    _items.emplace_back(std::move(item));
}

void ArrayObject::concat(Reference<ArrayObject> items)
{
    /* temporary buffer to hold objects from `items`,
     * the size may change after this, but not a big deal */
    std::vector<ObjectRef> refs;
    refs.reserve(items->size());

    /* copy from `items` first, to prevent holding two
     * locks at the same time, which might lead to dead-lock */
    {
        Utils::RWLock::Read _(items->_lock);
        refs.assign(items->items().begin(), items->items().end());
    }

    /* lock before modifying, and update modification counter */
    Utils::RWLock::Write _(_lock);
    _mods++;

    /* write into self, with write lock */
    _items.reserve(_items.size() + refs.size());
    _items.insert(std::end(_items), std::begin(refs), std::end(refs));
}

void ArrayObject::setItemAt(size_t index, ObjectRef value)
{
    /* lock before modifying */
    Utils::RWLock::Write _(_lock);

    /* check for array index */
    if (index >= _items.size())
    {
        throw Exceptions::IndexError(Utils::Strings::format(
            "Array index out of range [0, %zu): %zu",
            _items.size(),
            index
        ));
    }

    /* update the modification counter and content */
    _mods++;
    _items[index] = std::move(value);
}

void ArrayObject::removeItemAt(size_t index)
{
    /* lock before modifying */
    Utils::RWLock::Write _(_lock);

    /* check for array index */
    if (index >= _items.size())
    {
        throw Exceptions::IndexError(Utils::Strings::format(
            "Array index out of range [0, %zu): %zu",
            _items.size(),
            index
        ));
    }

    /* update the modification counter and content */
    _mods++;
    _items.erase(_items.begin() + index);
}

void ArrayObject::setSlice(size_t begin, size_t count, ssize_t step, ObjectRef items)
{
    /* check slicing step */
    if (step == 0)
        throw Exceptions::InternalError("Zero slicing step");

    /* make a copy of objects outside of lock */
    size_t size;
    ObjectRef *buffer;
    std::vector<ObjectRef> src;

    /* source is an array */
    if (items->isInstanceOf(ArrayTypeObject))
    {
        /* convert to array */
        auto array = items.as<ArrayObject>();

        /* get it's buffer and size */
        size = array->size();
        buffer = array->items().data();
    }

    /* source is a tuple */
    else if (items->isInstanceOf(TupleTypeObject))
    {
        /* convert to tuple */
        auto tuple = items.as<TupleObject>();

        /* get it's buffer and size */
        size = tuple->size();
        buffer = tuple->items();
    }

    /* generic object, try invoke it's iterator protocol */
    else
    {
        /* convert to an iterator */
        auto iter = items->type()->iterableIter(items);
        auto type = iter->type();

        /* get all items from iterator */
        try
        {
            for (;;)
                src.emplace_back(type->iterableNext(iter));
        }

        /* `StopIteration` is expected */
        catch (const Exceptions::StopIteration &)
        {
            /* should have no more items */
            /* this is expected, so ignore this exception */
        }

        /* get it's buffer and size */
        size = src.size();
        buffer = src.data();
    }

    /* lock before modifying, and update modification counter */
    Utils::RWLock::Write _(_lock);
    _mods++;

    /* check slicing range */
    if ((begin >= _items.size()) ||
        ((step < 0) && (begin < (count - 1) * -step)) ||
        ((step > 0) && (begin + (count - 1) * step >= _items.size())))
        throw Exceptions::InternalError("Array slicing out of range");

    /* special case #1: forward slicing assignment with step == 1 */
    if (step == 1)
    {
        /* special case #1.1: replace exact `count` items, just overwrite old values */
        if (size == count)
        {
            std::copy(buffer, buffer + size, _items.begin() + begin);
            return;
        }

        /* special case #1.2: not exactly `count` items, erase old values, then insert new values */
        _items.erase(_items.begin() + begin, _items.begin() + (begin + count));
        _items.insert(_items.begin() + begin, buffer, buffer + size);
        return;
    }

    /* special case #2: backward slicing removal with step == -1 */
    if (step == -1)
    {
        /* reversed source iterators */
        auto tail = std::make_reverse_iterator(buffer);
        auto head = std::make_reverse_iterator(buffer + size);

        /* special case #1.1: replace exact `count` items, just overwrite old values */
        if (size == count)
        {
            std::copy(head, tail, _items.begin() + (begin - count + 1));
            return;
        }

        /* special case #2.2: not exactly `count` items, erase old values, then insert new values in reverse */
        _items.erase(_items.begin() + (begin - count + 1), _items.begin() + (begin + 1));
        _items.insert(_items.begin() + (begin - count + 1), head, tail);
        return;
    }

    /* must have exact `count` items */
    if (size != count)
    {
        throw Exceptions::ValueError(Utils::Strings::format(
            "Attempt to assign sequence of size %zu to extended slice of size %zu",
            size,
            count
        ));
    }

    /* general case */
    for (auto iter = _items.begin() + begin; count; count--)
    {
        *iter = *buffer++;
        iter += step;
    }
}

void ArrayObject::removeSlice(size_t begin, size_t count, ssize_t step)
{
    /* check slicing step */
    if (step == 0)
        throw Exceptions::InternalError("Zero slicing step");

    /* lock before modifying, and update modification counter */
    Utils::RWLock::Write _(_lock);
    _mods++;

    /* check slicing range */
    if ((begin >= _items.size()) ||
        ((step < 0) && (begin < (count - 1) * -step)) ||
        ((step > 0) && (begin + (count - 1) * step >= _items.size())))
        throw Exceptions::InternalError("Array slicing out of range");

    /* special case #1: forward slicing removal with step == 1 */
    if (step == 1)
    {
        _items.erase(_items.begin() + begin, _items.begin() + (begin + count));
        return;
    }

    /* special case #2: backward slicing removal with step == -1 */
    if (step == -1)
    {
        _items.erase(_items.begin() + (begin - count + 1), _items.begin() + (begin + 1));
        return;
    }

    /* general case */
    while (count--)
    {
        /* erase an item at position `begin` */
        _items.erase(_items.begin() + begin);

        /* when forward slicing, elements moves every time,
         * so we need to adjust the `begin` pointer accordingly */
        if (step > 0)
            begin--;

        /* advance the pointer */
        begin += step;
    }
}

ObjectRef ArrayObject::itemAt(size_t index)
{
    /* lock before reading */
    Utils::RWLock::Read _(_lock);

    /* check for array index */
    if (index >= _items.size())
    {
        throw Exceptions::IndexError(Utils::Strings::format(
            "Array index out of range [0, %zu): %zu",
            _items.size(),
            index
        ));
    }

    /* fetch the item */
    return _items[index];
}

Reference<ArrayObject> ArrayObject::slice(size_t begin, size_t count, ssize_t step)
{
    /* create result array */
    size_t i = 0;
    Utils::RWLock::Read _(_lock);
    Reference<ArrayObject> result = ArrayObject::fromSize(count);

    /* fill each item */
    while ((i < count) && (begin < _items.size()))
    {
        /* copy one item into result */
        result->items()[i++] = _items[begin];

        /* integer might underflow when slicing
         * backwards, check before updating index */
        if ((step < 0) && (begin < -step))
            break;

        /* move to next item */
        begin += step;
    }

    /* should have exact `count` items */
    if (i != count)
        throw Exceptions::InternalError("Array slicing out of range");

    /* move to prevent copy */
    return std::move(result);
}

Reference<ArrayObject> ArrayObject::repeatCopy(size_t count)
{
    /* shortcut for times == 0 */
    if (count == 0)
        return ArrayObject::newEmpty();

    /* lock before reading */
    Utils::RWLock::Read _(_lock);
    std::vector<ObjectRef> result;

    /* reserve space for better performance */
    result.reserve(_items.size() * count);

    /* source iterators */
    auto end = std::end(_items);
    auto begin = std::begin(_items);

    /* fill them up */
    for (size_t i = 0; i < count; i++)
        result.insert(std::end(result), begin, end);

    /* move to prevent copy */
    return ArrayObject::fromArray(std::move(result));
}

Reference<ArrayObject> ArrayObject::concatCopy(Reference<ArrayObject> items)
{
    /* reserve space for better performance, the size of
     * the two arrays might change after this, but not a big deal */
    std::vector<ObjectRef> result;
    result.reserve(_items.size() + items->size());

    /* copy the left array (self), with read lock */
    {
        Utils::RWLock::Read _(_lock);
        result.insert(std::end(result), std::begin(_items), std::end(_items));
    }

    /* copy the right array (items), with read lock */
    {
        Utils::RWLock::Read _(items->_lock);
        result.insert(std::end(result), std::begin(items->_items), std::end(items->_items));
    }

    /* move to prevent copy */
    return ArrayObject::fromArray(std::move(result));
}

void ArrayObject::shutdown(void)
{
    ArrayTypeObject = nullptr;
    ArrayIteratorTypeObject = nullptr;
}

void ArrayObject::initialize(void)
{
    /* array type object */
    static ArrayType arrayType;
    static ArrayIteratorType arrayIteratorType;
    ArrayTypeObject = Reference<ArrayType>::refStatic(arrayType);
    ArrayIteratorTypeObject = Reference<ArrayIteratorType>::refStatic(arrayIteratorType);
}
}
