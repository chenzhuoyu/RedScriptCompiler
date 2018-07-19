#include <array>
#include <unordered_map>

#include "runtime/IntObject.h"
#include "runtime/BoolObject.h"
#include "runtime/StringObject.h"

#include "utils/Lists.h"
#include "utils/Strings.h"
#include "exceptions/TypeError.h"

namespace RedScript::Runtime
{
/* type object for string and string iterator */
TypeRef StringTypeObject;
TypeRef StringIteratorTypeObject;

/* special string constants and string pool */
static Reference<StringObject> _empty;
static std::array<Reference<StringObject>, 256> _chars;
static std::unordered_map<std::string, Reference<StringObject>> _interned;

/*** Native Object Protocol ***/

uint64_t StringType::nativeObjectHash(ObjectRef self)
{
    static std::hash<std::string> hash;
    return hash(self.as<StringObject>()->_value);
}

std::string StringType::nativeObjectStr(ObjectRef self)
{
    /* just give the string itself */
    return self.as<StringObject>()->_value;
}

std::string StringType::nativeObjectRepr(ObjectRef self)
{
    const auto &str = self.as<StringObject>()->_value;
    return Utils::Strings::repr(str.data(), str.size());
}

bool StringType::nativeObjectIsTrue(ObjectRef self)
{
    /* non-empty string represents "true" */
    return !self.as<StringObject>()->_value.empty();
}

/*** Native Numeric Protocol ***/

ObjectRef StringType::nativeNumericAdd(ObjectRef self, ObjectRef other)
{
    /* type check */
    if (other->isNotInstanceOf(StringTypeObject))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "Can only concatenate string (not \"%s\") to string",
            other->type()->name()
        ));
    }

    /* build the result string */
    return StringObject::fromString(
        self.as<StringObject>()->_value +
        other.as<StringObject>()->_value
    );
}

ObjectRef StringType::nativeNumericMul(ObjectRef self, ObjectRef other)
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
    size_t times;
    Reference<IntObject> val = other.as<IntObject>();
    Reference<StringObject> string = self.as<StringObject>();

    /* must be an unsigned integer */
    if (!(val->isSafeUInt()))
        throw Exceptions::ValueError("Repeating count must be an unsigned integer");

    /* get the repeating count */
    switch ((times = val->toUInt()))
    {
        /* repeat 0 times, that gives an empty string */
        case 0:
            return StringObject::newEmpty();

        /* repeat 1 time, that gives the string itself;
         * since strings are immutable, it is safe to return a shared copy */
        case 1:
            return self;

        /* repeat the string `times` that many times */
        default:
        {
            std::vector<std::string> list(times, string->_value);
            return StringObject::fromString(Utils::Strings::join(list));
        }
    }
}

/*** Native Iterator Protocol ***/

ObjectRef StringType::nativeIterableIter(ObjectRef self)
{
    /* create an iterator from string */
    return Object::newObject<StringIteratorObject>(self.as<StringObject>());
}

ObjectRef StringIteratorType::nativeIterableNext(ObjectRef self)
{
    /* get the next string */
    return self.as<StringIteratorObject>()->next();
}

/*** Native Sequence Protocol ***/

ObjectRef StringType::nativeSequenceLen(ObjectRef self)
{
    /* get the length, and wrap with integer */
    return IntObject::fromUInt(self.as<StringObject>()->size());
}

ObjectRef StringType::nativeSequenceGetItem(ObjectRef self, ObjectRef other)
{
    /* extract the item */
    Reference<StringObject> string = self.as<StringObject>();
    return StringObject::fromStringInterned(std::string(1, string->_value[Utils::Lists::indexConstraint(string, other)]));
}

ObjectRef StringType::nativeSequenceGetSlice(ObjectRef self, ObjectRef begin, ObjectRef end, ObjectRef step)
{
    /* parse the slice range */
    auto string = self.as<StringObject>();
    Utils::Lists::Slice slice = Utils::Lists::sliceConstraint(string, begin, end, step);

    /* special case #1: forward slicing with slice.step == 1 */
    if (slice.step == 1)
        return StringObject::fromString(string->_value.substr(slice.begin, slice.count));

    /* special case #2: backward slicing with slice.step == -1 */
    if (slice.step == -1)
    {
        auto p = slice.begin - slice.count + 1;
        auto s = string->_value.substr(p, slice.count);
        return StringObject::fromString(std::string(s.rbegin(), s.rend()));
    }

    /* general case, reserve space for better performance */
    std::string result;
    result.reserve(slice.count);

    /* fill each item */
    while ((result.size() < slice.count) && (slice.begin < string->size()))
    {
        /* copy one character into result */
        result += string->_value[slice.begin];

        /* integer might underflow when slicing
         * backwards, check before updating index */
        if ((slice.step < 0) && (slice.begin < -slice.step))
            break;

        /* move to next item */
        slice.begin += slice.step;
    }

    /* should have exact `count` characters */
    if (result.size() != slice.count)
        throw Exceptions::InternalError("String slicing out of range");

    /* wrap with string object */
    return StringObject::fromString(result);
}

/*** Comparator Protocol ***/

#define BOOL_OP(op) {                                                           \
    return BoolObject::fromBool(                                                \
        other->type()->objectIsInstanceOf(other, StringTypeObject) &&           \
        (self.as<StringObject>()->_value op other.as<StringObject>()->_value)   \
    );                                                                          \
}

ObjectRef StringType::nativeComparableEq (ObjectRef self, ObjectRef other) BOOL_OP(==)
ObjectRef StringType::nativeComparableLt (ObjectRef self, ObjectRef other) BOOL_OP(< )
ObjectRef StringType::nativeComparableGt (ObjectRef self, ObjectRef other) BOOL_OP(> )
ObjectRef StringType::nativeComparableNeq(ObjectRef self, ObjectRef other) BOOL_OP(!=)
ObjectRef StringType::nativeComparableLeq(ObjectRef self, ObjectRef other) BOOL_OP(<=)
ObjectRef StringType::nativeComparableGeq(ObjectRef self, ObjectRef other) BOOL_OP(>=)

#undef BOOL_OP

ObjectRef StringType::nativeComparableCompare(ObjectRef self, ObjectRef other)
{
    /* string type check */
    if (!(other->type()->objectIsInstanceOf(other, StringTypeObject)))
    {
        throw Exceptions::TypeError(Utils::Strings::format(
            "\"%s\" is not comparable with \"str\"",
            other->type()->name()
        ));
    }

    /* perform total-order comparison */
    return IntObject::fromInt(self.as<StringObject>()->_value.compare(other.as<StringObject>()->_value));
}

ObjectRef StringType::nativeComparableContains(ObjectRef self, ObjectRef other)
{
    return BoolObject::fromBool(
        other->type()->objectIsInstanceOf(other, StringTypeObject) &&
        (self.as<StringObject>()->_value.find(other.as<StringObject>()->_value) != std::string::npos)
    );
}

ObjectRef StringObject::newEmpty(void)
{
    /* we have an empty string already */
    return _empty;
}

ObjectRef StringObject::fromString(const std::string &value)
{
    /* empty string */
    if (value.empty())
        return _empty;

    /* single-character strings */
    if (value.size() == 1)
        return _chars[value[0]];

    /* otherwise create a new string object */
    return Object::newObject<StringObject>(value);
}

ObjectRef StringObject::fromStringInterned(const std::string &value)
{
    /* empty string */
    if (value.empty())
        return _empty;

    /* single-character strings */
    if (value.size() == 1)
        return _chars[value[0]];

    /* intern string if not already */
    if (_interned.find(value) == _interned.end())
        _interned.emplace(value, Object::newObject<StringObject>(value));

    /* read from pool */
    return _interned.at(value);
}

void StringObject::shutdown(void)
{
    /* clear all character strings */
    for (int c = 0; c < 256; c++)
        _chars[c] = nullptr;

    /* clear all interned strings */
    _interned.clear();
}

void StringObject::initialize(void)
{
    /* string type object */
    static StringType stringType;
    static StringIteratorType stringIteratorType;
    StringTypeObject = Reference<StringType>::refStatic(stringType);
    StringIteratorTypeObject = Reference<StringIteratorType>::refStatic(stringIteratorType);

    /* empty string */
    static StringObject empty("");
    _empty = Reference<StringObject>::refStatic(empty);

    /* single-character strings */
    for (int c = 0; c < 256; c++)
        _chars[c] = Object::newObject<StringObject>(std::string(1, c));
}
}
