#include "utils/Integer.h"
#include "utils/Strings.h"

#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/ArrayObject.h"

#include "exceptions/TypeError.h"
#include "exceptions/IndexError.h"
#include "exceptions/ValueError.h"
#include "exceptions/InternalError.h"

namespace RedScript::Runtime
{
/* type object for array and array iterator */
TypeRef ArrayTypeObject;
TypeRef ArrayIteratorTypeObject;

/*** Object Protocol ***/

uint64_t ArrayType::objectHash(ObjectRef self)
{
    /* arrays are not hashable */
    throw Exceptions::TypeError("Unhashable type \"array\"");
}

std::string ArrayType::objectRepr(ObjectRef self)
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

bool ArrayType::objectIsTrue(ObjectRef self)
{
    /* non-empty arrays represents true */
    return self.as<ArrayObject>()->size() != 0;
}

/*** Numeric Protocol ***/

ObjectRef ArrayType::numericAdd(ObjectRef self, ObjectRef other)
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

ObjectRef ArrayType::numericMul(ObjectRef self, ObjectRef other)
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

ObjectRef ArrayType::numericIncAdd(ObjectRef self, ObjectRef other)
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

ObjectRef ArrayType::numericIncMul(ObjectRef self, ObjectRef other)
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

/*** Iterator Protocol ***/

ObjectRef ArrayType::iterableIter(ObjectRef self)
{
    /* create an iterator from array */
    return Object::newObject<ArrayIteratorObject>(self.as<ArrayObject>());
}

ObjectRef ArrayIteratorType::iterableNext(ObjectRef self)
{
    /* get the next object */
    return self.as<ArrayIteratorObject>()->next();
}

/*** Sequence Protocol ***/

ObjectRef ArrayType::sequenceLen(ObjectRef self)
{
    /* get the length, and wrap with integer */
    return IntObject::fromUInt(self.as<ArrayObject>()->size());
}

void ArrayType::sequenceDelItem(ObjectRef self, ObjectRef other)
{
    /* type check */
    if (other->isNotInstanceOf(IntTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Array index must be an integer, not \"%s\"",
            other->type()->name()
        ));
    }

    /* convert to correct type */
    Reference<IntObject> val = other.as<IntObject>();
    Reference<ArrayObject> array = self.as<ArrayObject>();

    /* remove item from array */
    array->removeItemAt(Utils::Lists::indexConstraint(array, val));
}

ObjectRef ArrayType::sequenceGetItem(ObjectRef self, ObjectRef other)
{
    /* type check */
    if (other->isNotInstanceOf(IntTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Array index must be an integer, not \"%s\"",
            other->type()->name()
        ));
    }

    /* convert to correct type */
    Reference<IntObject> val = other.as<IntObject>();
    Reference<ArrayObject> array = self.as<ArrayObject>();

    /* get item from array */
    return array->itemAt(Utils::Lists::indexConstraint(array, val));
}

void ArrayType::sequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third)
{
    /* type check */
    if (second->isNotInstanceOf(IntTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Array index must be an integer, not \"%s\"",
            second->type()->name()
        ));
    }

    /* convert to correct type */
    Reference<IntObject> val = second.as<IntObject>();
    Reference<ArrayObject> array = self.as<ArrayObject>();

    /* set item in array */
    array->setItemAt(Utils::Lists::indexConstraint(array, val), std::move(third));
}

/*** Comparator Protocol ***/

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

ObjectRef ArrayType::comparableEq (ObjectRef self, ObjectRef other) BOOL_CMP(==)
ObjectRef ArrayType::comparableLt (ObjectRef self, ObjectRef other) BOOL_CMP(< )
ObjectRef ArrayType::comparableGt (ObjectRef self, ObjectRef other) BOOL_CMP(> )
ObjectRef ArrayType::comparableNeq(ObjectRef self, ObjectRef other) BOOL_CMP(!=)
ObjectRef ArrayType::comparableLeq(ObjectRef self, ObjectRef other) BOOL_CMP(<=)
ObjectRef ArrayType::comparableGeq(ObjectRef self, ObjectRef other) BOOL_CMP(>=)

#undef BOOL_CMP

ObjectRef ArrayType::comparableCompare(ObjectRef self, ObjectRef other)
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

ObjectRef ArrayType::comparableContains(ObjectRef self, ObjectRef other)
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

void ArrayObject::initialize(void)
{
    /* array type object */
    static ArrayType arrayType;
    static ArrayIteratorType arrayIteratorType;
    ArrayTypeObject = Reference<ArrayType>::refStatic(arrayType);
    ArrayIteratorTypeObject = Reference<ArrayIteratorType>::refStatic(arrayIteratorType);
}
}
