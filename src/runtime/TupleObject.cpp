#include "utils/Integer.h"
#include "utils/Strings.h"

#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/TupleObject.h"

#include "exceptions/TypeError.h"
#include "exceptions/IndexError.h"
#include "exceptions/ValueError.h"
#include "exceptions/InternalError.h"

namespace RedScript::Runtime
{
/* type object for tuple and tuple iterator */
TypeRef TupleTypeObject;
TypeRef TupleIteratorTypeObject;

/*** Object Protocol ***/

uint64_t TupleType::objectHash(ObjectRef self)
{
    /* convert to tuple object */
    uint64_t hash = 5381;
    Reference<TupleObject> tuple = self.as<TupleObject>();

    /* hash each item */
    for (size_t i = 0; i < tuple->size(); i++)
    {
        hash *= 32;
        hash ^= tuple->items()[i]->type()->objectHash(tuple->items()[i]);
    }

    /* invert the hash */
    return ~hash;
}

std::string TupleType::objectRepr(ObjectRef self)
{
    /* scope control */
    Object::Repr repr(self);
    Reference<TupleObject> tuple = self.as<TupleObject>();
    std::vector<std::string> items;

    /* recursion detected */
    if (repr.isExists())
        return "(...)";

    /* hash each item */
    for (size_t i = 0; i < tuple->size(); i++)
    {
        auto item = tuple->items()[i];
        items.emplace_back(item->type()->objectRepr(item));
    }

    /* special case when tuple contains only 1 item */
    if (items.size() == 1)
        return Utils::Strings::format("(%s,)", items[0]);
    else
        return Utils::Strings::format("(%s)", Utils::Strings::join(items, ", "));
}

bool TupleType::objectIsTrue(ObjectRef self)
{
    /* non-empty tuple represents true */
    return self.as<TupleObject>()->size() != 0;
}

/*** Numeric Protocol ***/

ObjectRef TupleType::numericAdd(ObjectRef self, ObjectRef other)
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
    Reference<TupleObject> result = TupleObject::fromSize(left->size() + right->size());

    /* fill each item form left tuple */
    for (size_t i = 0; i < left->size(); i++)
        result->items()[i] = left->items()[i];

    /* fill each item form right tuple */
    for (size_t i = 0; i < right->size(); i++)
        result->items()[i + left->size()] = right->items()[i];

    /* move to prevent copy */
    return std::move(result);
}

ObjectRef TupleType::numericMul(ObjectRef self, ObjectRef other)
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
    size_t count = tuple->size();

    /* shortcut for times == 0 and 1 */
    if (times == 0)
        return TupleObject::newEmpty();

    /* since tuples are immutable, it is safe to return a shared copy */
    if (times == 1)
        return other;

    /* create the result tuple */
    size_t newCount = count * times;
    Reference<TupleObject> result = TupleObject::fromSize(newCount);

    /* fill them up */
    for (size_t i = 0; i < times; i++)
        for (size_t j = 0; j < count; j++)
            result->items()[i * count + j] = tuple->items()[j];

    /* move to prevent copy */
    return std::move(result);
}

/*** Iterator Protocol ***/

ObjectRef TupleType::iterableIter(ObjectRef self)
{
    /* create an iterator from tuple */
    return Object::newObject<TupleIteratorObject>(self.as<TupleObject>());
}

ObjectRef TupleIteratorType::iterableNext(ObjectRef self)
{
    /* get the next object */
    return self.as<TupleIteratorObject>()->next();
}

/*** Sequence Protocol ***/

ObjectRef TupleType::sequenceLen(ObjectRef self)
{
    /* get the length, and wrap with integer */
    return IntObject::fromUInt(self.as<TupleObject>()->size());
}

ObjectRef TupleType::sequenceGetItem(ObjectRef self, ObjectRef other)
{
    /* integer type check */
    if (other->isNotInstanceOf(IntTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Tuple index must be integers, not \"%s\"",
            other->type()->name()
        ));
    }

    /* convert to correct types */
    Reference<IntObject> val = other.as<IntObject>();
    Reference<TupleObject> tuple = self.as<TupleObject>();

    /* check index type */
    if (!(val->isSafeUInt()))
    {
        throw Exceptions::IndexError(Utils::Strings::format(
            "Tuple index out of range [0, %lu): %s",
            tuple->size(),
            val->value().toString()
        ));
    }

    /* extract the index as unsigned integer */
    uint64_t index = val->toUInt();

    /* check index range */
    if (index >= tuple->size())
    {
        throw Exceptions::IndexError(Utils::Strings::format(
            "Tuple index out of range [0, %lu): %lu",
            tuple->size(),
            index
        ));
    }

    /* extract the item */
    ObjectRef item = tuple->items()[index];

    /* must not be null */
    if (item.isNull())
        throw Exceptions::InternalError("Tuple contains null value");

    /* move to prevent copy */
    return std::move(item);
}

/*** Comparator Protocol ***/

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

ObjectRef TupleType::comparableEq (ObjectRef self, ObjectRef other) BOOL_CMP(==)
ObjectRef TupleType::comparableLt (ObjectRef self, ObjectRef other) BOOL_CMP(< )
ObjectRef TupleType::comparableGt (ObjectRef self, ObjectRef other) BOOL_CMP(> )
ObjectRef TupleType::comparableNeq(ObjectRef self, ObjectRef other) BOOL_CMP(!=)
ObjectRef TupleType::comparableLeq(ObjectRef self, ObjectRef other) BOOL_CMP(<=)
ObjectRef TupleType::comparableGeq(ObjectRef self, ObjectRef other) BOOL_CMP(>=)

#undef BOOL_CMP

ObjectRef TupleType::comparableCompare(ObjectRef self, ObjectRef other)
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

ObjectRef TupleType::comparableContains(ObjectRef self, ObjectRef other)
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
        if (!(_items[i].isNull()))
            visit(_items[i]);
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
