#include "utils/Integer.h"
#include "utils/Strings.h"

#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/TupleObject.h"

namespace RedScript::Runtime
{
/* type object for tuple and tuple iterator */
TypeRef TupleTypeObject;
TypeRef TupleIteratorTypeObject;

/*** Native Object Protocol ***/

uint64_t TupleType::nativeObjectHash(ObjectRef self)
{
    /* convert to tuple object */
    uint64_t hash = 5381;
    Reference<TupleObject> tuple = self.as<TupleObject>();

    /* hash each item */
    for (size_t i = 0; i < tuple->_size; i++)
    {
        hash *= 32;
        hash ^= tuple->_items[i]->type()->objectHash(tuple->_items[i]);
    }

    /* invert the hash */
    return ~hash;
}

std::string TupleType::nativeObjectRepr(ObjectRef self)
{
    /* scope control */
    Object::Repr repr(self);
    Reference<TupleObject> tuple = self.as<TupleObject>();
    std::vector<std::string> items;

    /* recursion detected */
    if (repr.isExists())
        return "(...)";

    /* hash each item */
    for (size_t i = 0; i < tuple->_size; i++)
    {
        auto item = tuple->_items[i];
        items.emplace_back(item->type()->objectRepr(item));
    }

    /* special case when tuple contains only 1 item */
    if (items.size() == 1)
        return Utils::Strings::format("(%s,)", items[0]);
    else
        return Utils::Strings::format("(%s)", Utils::Strings::join(items, ", "));
}

bool TupleType::nativeObjectIsTrue(ObjectRef self)
{
    /* non-empty tuple represents true */
    return self.as<TupleObject>()->_size != 0;
}

/*** Native Numeric Protocol ***/

ObjectRef TupleType::nativeNumericAdd(ObjectRef self, ObjectRef other)
{
    /* type check */
    if (other->isNotInstanceOf(TupleTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Can only concatenate tuple (not \"%s\") to tuple",
            other->type()->name()
        ));
    }

    /* convert to tuple */
    Reference<TupleObject> left = self.as<TupleObject>();
    Reference<TupleObject> right = other.as<TupleObject>();
    Reference<TupleObject> result = TupleObject::fromSize(left->_size + right->_size);

    /* fill each item form left tuple */
    for (size_t i = 0; i < left->_size; i++)
        result->_items[i] = left->_items[i];

    /* fill each item form right tuple */
    for (size_t i = 0; i < right->_size; i++)
        result->_items[i + left->_size] = right->_items[i];

    /* move to prevent copy */
    return std::move(result);
}

ObjectRef TupleType::nativeNumericMul(ObjectRef self, ObjectRef other)
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
    Reference<TupleObject> tuple = self.as<TupleObject>();

    /* must be an unsigned integer */
    if (!(val->isSafeUInt()))
        throw Exceptions::ValueError("Repeating count must be an unsigned integer");

    /* get the repeating count */
    size_t times = val->toUInt();
    size_t length = tuple->_size;

    /* shortcut for times == 0 and 1 */
    if (times == 0)
        return TupleObject::newEmpty();

    /* since tuples are immutable, it is safe to return a shared copy */
    if (times == 1)
        return self;

    /* create the result tuple */
    size_t newCount = length * times;
    Reference<TupleObject> result = TupleObject::fromSize(newCount);

    /* fill them up */
    for (size_t i = 0; i < times; i++)
        for (size_t j = 0; j < length; j++)
            result->_items[i * length + j] = tuple->_items[j];

    /* move to prevent copy */
    return std::move(result);
}

/*** Native Iterator Protocol ***/

ObjectRef TupleType::nativeIterableIter(ObjectRef self)
{
    /* create an iterator from tuple */
    return Object::newObject<TupleIteratorObject>(self.as<TupleObject>());
}

ObjectRef TupleIteratorType::nativeIterableNext(ObjectRef self)
{
    /* get the next object */
    return self.as<TupleIteratorObject>()->next();
}

/*** Native Sequence Protocol ***/

ObjectRef TupleType::nativeSequenceLen(ObjectRef self)
{
    /* get the length, and wrap with integer */
    return IntObject::fromUInt(self.as<TupleObject>()->_size);
}

ObjectRef TupleType::nativeSequenceGetItem(ObjectRef self, ObjectRef other)
{
    /* extract the item */
    auto tuple = self.as<TupleObject>();
    auto result = tuple->_items[Utils::Lists::indexConstraint(tuple, other)];

    /* must not be null */
    if (result.isNull())
        throw Exceptions::InternalError("Tuple contains null value");

    /* move to prevent copy */
    return std::move(result);
}

ObjectRef TupleType::nativeSequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)
{
    /* parse the slice range */
    auto tuple = self.as<TupleObject>();
    Utils::Lists::Slice slice = Utils::Lists::sliceConstraint(tuple, begin, end, step);

    /* create result tuple */
    size_t i = 0;
    size_t size = tuple->_size;
    Reference<TupleObject> result = TupleObject::fromSize(slice.count);

    /* fill each item */
    while ((i < slice.count) && (slice.begin < size))
    {
        /* copy one item into result */
        result->_items[i++] = tuple->_items[slice.begin];

        /* integer might underflow when slicing
         * backwards, check before updating index */
        if ((slice.step < 0) && (slice.begin < -slice.step))
            break;

        /* move to next item */
        slice.begin += slice.step;
    }

    /* should have exact `count` items */
    if (i != slice.count)
        throw Exceptions::InternalError("Tuple slicing out of range");

    /* move to prevent copy */
    return std::move(result);
}

/*** Native Comparator Protocol ***/

#define BOOL_CMP(op) {                                              \
    /* object type checking */                                      \
    if (other->isNotInstanceOf(TupleTypeObject))                    \
        return FalseObject;                                         \
                                                                    \
    /* perform comparison */                                        \
    return BoolObject::fromBool(Utils::Lists::totalOrderCompare(    \
        self.as<TupleObject>(),                                     \
        other.as<TupleObject>()                                     \
    ) op 0);                                                        \
}

ObjectRef TupleType::nativeComparableEq (ObjectRef self, ObjectRef other) BOOL_CMP(==)
ObjectRef TupleType::nativeComparableLt (ObjectRef self, ObjectRef other) BOOL_CMP(< )
ObjectRef TupleType::nativeComparableGt (ObjectRef self, ObjectRef other) BOOL_CMP(> )
ObjectRef TupleType::nativeComparableNeq(ObjectRef self, ObjectRef other) BOOL_CMP(!=)
ObjectRef TupleType::nativeComparableLeq(ObjectRef self, ObjectRef other) BOOL_CMP(<=)
ObjectRef TupleType::nativeComparableGeq(ObjectRef self, ObjectRef other) BOOL_CMP(>=)

#undef BOOL_CMP

ObjectRef TupleType::nativeComparableCompare(ObjectRef self, ObjectRef other)
{
    /* object type checking */
    if (other->isNotInstanceOf(TupleTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"tuple\" and \"%s\" objects are not comparable",
            other->type()->name()
        ));
    }

    /* call the total-order comparing function */
    return IntObject::fromInt(Utils::Lists::totalOrderCompare(self.as<TupleObject>(), other.as<TupleObject>()));
}

ObjectRef TupleType::nativeComparableContains(ObjectRef self, ObjectRef other)
{
    Reference<TupleObject> tuple = self.as<TupleObject>();
    return BoolObject::fromBool(Utils::Lists::contains(std::move(tuple), std::move(other)));
}

TupleObject::TupleObject(size_t size) :
    Object(TupleTypeObject),
    _size(size),
    _items(new ObjectRef[size])
{
    if (size > 0)
        track();
}

void TupleObject::referenceClear(void)
{
    for (size_t i = 0; i < _size; i++)
        _items[i] = nullptr;
}

void TupleObject::referenceTraverse(VisitFunction visit)
{
    for (size_t i = 0; i < _size; i++)
        if (_items[i].isNotNull())
            visit(_items[i]);
}

void TupleObject::shutdown(void)
{
    TupleTypeObject = nullptr;
    TupleIteratorTypeObject = nullptr;
}

void TupleObject::initialize(void)
{
    /* tuple type object */
    static TupleType tupleType;
    static TupleIteratorType tupleIteratorType;
    TupleTypeObject = Reference<TupleType>::refStatic(tupleType);
    TupleIteratorTypeObject = Reference<TupleIteratorType>::refStatic(tupleIteratorType);
}
}
