#include "runtime/ArrayObject.h"
#include "exceptions/TypeError.h"

namespace RedScript::Runtime
{
/* type object for arrays */
TypeRef ArrayTypeObject;

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

    /* convert to tuple */
    Reference<ArrayObject> left = self.as<ArrayObject>();
    Reference<ArrayObject> right = other.as<ArrayObject>();
    Reference<ArrayObject> result = ArrayObject::fromSize(left->size() + right->size());

    /* fill each item form left tuple */
    for (size_t i = 0; i < left->size(); i++)
        result->items()[i] = left->items()[i];

    /* fill each item form right tuple */
    for (size_t i = 0; i < right->size(); i++)
        result->items()[i + left->size()] = right->items()[i];

    /* move to prevent copy */
    return std::move(result);
}

ObjectRef ArrayType::numericMul(ObjectRef self, ObjectRef other)
{
    return Type::numericMul(self, other);
}

ObjectRef ArrayType::numericIncAdd(ObjectRef self, ObjectRef other)
{
    return self;
}

ObjectRef ArrayType::numericIncMul(ObjectRef self, ObjectRef other)
{
    return self;
}

/*** Iterator Protocol ***/

ObjectRef ArrayType::iterableIter(ObjectRef self)
{
    return Type::iterableIter(self);
}

/*** Sequence Protocol ***/

ObjectRef ArrayType::sequenceLen(ObjectRef self)
{
    return Type::sequenceLen(self);
}

ObjectRef ArrayType::sequenceGetItem(ObjectRef self, ObjectRef other)
{
    return Type::sequenceGetItem(self, other);
}

/*** Comparator Protocol ***/

ObjectRef ArrayType::comparableEq(ObjectRef self, ObjectRef other)
{
    return Type::comparableEq(self, other);
}

ObjectRef ArrayType::comparableLt(ObjectRef self, ObjectRef other)
{
    return Type::comparableLt(self, other);
}

ObjectRef ArrayType::comparableGt(ObjectRef self, ObjectRef other)
{
    return Type::comparableGt(self, other);
}

ObjectRef ArrayType::comparableNeq(ObjectRef self, ObjectRef other)
{
    return Type::comparableNeq(self, other);
}

ObjectRef ArrayType::comparableLeq(ObjectRef self, ObjectRef other)
{
    return Type::comparableLeq(self, other);
}

ObjectRef ArrayType::comparableGeq(ObjectRef self, ObjectRef other)
{
    return Type::comparableGeq(self, other);
}

ObjectRef ArrayType::comparableCompare(ObjectRef self, ObjectRef other)
{
    return Type::comparableCompare(self, other);
}

ObjectRef ArrayType::comparableContains(ObjectRef self, ObjectRef other)
{
    return Type::comparableContains(self, other);
}

void ArrayObject::initialize(void)
{
    /* array type object */
    static ArrayType arrayType;
    ArrayTypeObject = Reference<ArrayType>::refStatic(arrayType);
}
}
