#ifndef REDSCRIPT_UTILS_NFI_H
#define REDSCRIPT_UTILS_NFI_H

#include <map>
#include <array>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

#include "runtime/Object.h"
#include "runtime/IntObject.h"
#include "runtime/MapObject.h"
#include "runtime/BoolObject.h"
#include "runtime/NullObject.h"
#include "runtime/ArrayObject.h"
#include "runtime/TupleObject.h"
#include "runtime/StringObject.h"
#include "runtime/DecimalObject.h"
#include "runtime/ExceptionObject.h"

#include "utils/Decimal.h"
#include "utils/Integer.h"
#include "utils/Strings.h"

/*** Native Function Interface ***/

namespace RedScript::Utils::NFI
{
typedef std::vector<std::string> KeywordNames;
typedef std::vector<Runtime::ObjectRef> DefaultValues;
typedef Runtime::Reference<Runtime::MapObject> KeywordArgs;
typedef Runtime::Reference<Runtime::TupleObject> VariadicArgs;

namespace Details
{
template <typename T>
struct IsMutableLValueReference
{
    /* is L-value reference, and is not constant, means mutable reference */
    static constexpr bool value = std::is_lvalue_reference_v<T> && !(std::is_const_v<std::remove_reference_t<T>>);
};

/** Object Boxer **/

template <size_t>
struct IntegerBoxer;

template <>
struct IntegerBoxer<sizeof(int8_t)>
{
    static Runtime::ObjectRef boxSigned(int8_t value)    { return Runtime::IntObject::fromInt(value);  }
    static Runtime::ObjectRef boxUnsigned(uint8_t value) { return Runtime::IntObject::fromUInt(value); }
};

template <>
struct IntegerBoxer<sizeof(int16_t)>
{
    static Runtime::ObjectRef boxSigned(int16_t value)    { return Runtime::IntObject::fromInt(value);  }
    static Runtime::ObjectRef boxUnsigned(uint16_t value) { return Runtime::IntObject::fromUInt(value); }
};

template <>
struct IntegerBoxer<sizeof(int32_t)>
{
    static Runtime::ObjectRef boxSigned(int32_t value)    { return Runtime::IntObject::fromInt(value);  }
    static Runtime::ObjectRef boxUnsigned(uint32_t value) { return Runtime::IntObject::fromUInt(value); }
};

template <>
struct IntegerBoxer<sizeof(int64_t)>
{
    static Runtime::ObjectRef boxSigned(int64_t value)    { return Runtime::IntObject::fromInt(value);  }
    static Runtime::ObjectRef boxUnsigned(uint64_t value) { return Runtime::IntObject::fromUInt(value); }
};

template <bool Signed, bool Unsigned, typename T>
struct BoxerHelper
{
    static Runtime::ObjectRef box(T &&value)
    {
        /* types that not recognized, try casting to `ObjectRef` */
        return static_cast<Runtime::ObjectRef>(value);
    }
};

template <typename T>
struct BoxerHelper<true, false, T>
{
    static Runtime::ObjectRef box(T &&value)
    {
        /* signed integers */
        return IntegerBoxer<sizeof(T)>::boxSigned(std::forward<T>(value));
    }
};

template <typename T>
struct BoxerHelper<false, true, T>
{
    static Runtime::ObjectRef box(T &&value)
    {
        /* unsigned integers */
        return IntegerBoxer<sizeof(T)>::boxUnsigned(std::forward<T>(value));
    }
};

/* generic boxer */
template <typename T>
struct Boxer
{
    static Runtime::ObjectRef box(T &&value)
    {
        /* doesn't support pointers, R-value references or mutable references */
        static_assert(!(std::is_pointer_v<T>), "Pointers are not supported");
        static_assert(!(std::is_rvalue_reference_v<T>), "R-value references are not supported");
        static_assert(!(IsMutableLValueReference<T>::value), "Mutable references are not supported");

        /* box the value */
        return BoxerHelper<
            std::is_signed_v<std::decay_t<T>>,      /* signed-integer check */
            std::is_unsigned_v<std::decay_t<T>>,    /* unsigned-integer check */
            std::decay_t<T>
        >::box(std::forward<T>(value));
    }
};

/* reference boxer */
template <typename T>
struct Boxer<const T &>
{
    static Runtime::ObjectRef box(const T &value)
    {
        /* call the reference-removed version */
        return Boxer<std::decay_t<T>>::box(value);
    }
};

/* simple types */
template <> struct Boxer<bool>               { static Runtime::ObjectRef box(bool value)               { return Runtime::BoolObject::fromBool(value);       }};
template <> struct Boxer<float>              { static Runtime::ObjectRef box(float value)              { return Runtime::DecimalObject::fromDouble(value);  }};
template <> struct Boxer<double>             { static Runtime::ObjectRef box(double value)             { return Runtime::DecimalObject::fromDouble(value);  }};
template <> struct Boxer<long double>        { static Runtime::ObjectRef box(long double value)        { return Runtime::DecimalObject::fromDecimal(value); }};
template <> struct Boxer<Runtime::ObjectRef> { static Runtime::ObjectRef box(Runtime::ObjectRef value) { return value;                                      }};

/* high-precision decimal */
template <>
struct Boxer<Decimal>
{
    static Runtime::ObjectRef box(Decimal &&value)      { return Runtime::DecimalObject::fromDecimal(std::move(value)); }
    static Runtime::ObjectRef box(const Decimal &value) { return Runtime::DecimalObject::fromDecimal(value); }
};

/* high-precision integer */
template <>
struct Boxer<Integer>
{
    static Runtime::ObjectRef box(Integer &&value)      { return Runtime::IntObject::fromInteger(std::move(value)); }
    static Runtime::ObjectRef box(const Integer &value) { return Runtime::IntObject::fromInteger(value); }
};

/* STL string */
template <>
struct Boxer<std::string>
{
    static Runtime::ObjectRef box(std::string &&value)      { return Runtime::StringObject::fromString(std::move(value)); }
    static Runtime::ObjectRef box(const std::string &value) { return Runtime::StringObject::fromString(value); }
};

/* STL vector (arrays) */
template <typename Item>
struct Boxer<std::vector<Item>>
{
    static Runtime::ObjectRef box(const std::vector<Item> &value)
    {
        /* create the result tuple */
        size_t i = 0;
        Runtime::Reference<Runtime::TupleObject> result = Runtime::TupleObject::fromSize(value.size());

        /* box each item */
        while (i < value.size())
        {
            result->items()[i] = Boxer<Item>::box(value[i]);
            i++;
        }

        /* move to prevent copy */
        return std::move(result);
    }
};

/* map boxer helper */
template <typename Map, typename Key, typename Value>
struct MapBoxer
{
    static Runtime::ObjectRef box(const Map &value)
    {
        /* create the result map, create as ordered map */
        Runtime::Reference<Runtime::MapObject> result = Runtime::MapObject::newOrdered();

        /* box each item */
        for (const auto &item : value)
        {
            result->insert(
                Boxer<Key>::box(item.first),
                Boxer<Value>::box(item.second)
            );
        }

        /* move to prevent copy */
        return std::move(result);
    }
};

/* STL map (tree maps) */
template <typename Key, typename Value>
struct Boxer<std::map<Key, Value>>
{
    static Runtime::ObjectRef box(const std::map<Key, Value> &value)
    {
        /* call the map boxer, tree maps */
        return MapBoxer<std::map<Key, Value>, Key, Value>::box(value);
    }
};

/* STL unordered map (hash maps) */
template <typename Key, typename Value>
struct Boxer<std::unordered_map<Key, Value>>
{
    static Runtime::ObjectRef box(const std::unordered_map<Key, Value> &value)
    {
        /* call the map boxer, hash maps */
        return MapBoxer<std::unordered_map<Key, Value>, Key, Value>::box(value);
    }
};

/** Object Unboxer **/

template <size_t I>
static inline Runtime::Exceptions::TypeError TypeCheckFailed(Runtime::TypeRef type, const std::string &name, Runtime::ObjectRef object)
{
    return Runtime::Exceptions::TypeError(Utils::Strings::format(
        "Argument at position %zu%s must be a \"%s\" object, not \"%s\"",
        I,
        name.empty() ? "" : Utils::Strings::format("(%s)", name),
        type->name(),
        object->type()->name()
    ));
}

template <size_t I>
static inline Runtime::Exceptions::ValueError ValueCheckFailed(Runtime::ObjectRef object, const std::string &name)
{
    return Runtime::Exceptions::ValueError(Utils::Strings::format(
        "Argument at position %zu%s holds an invalid value : %s",
        I,
        name.empty() ? "" : Utils::Strings::format("(%s)", name),
        object->type()->objectRepr(object)
    ));
}

template <size_t, size_t>
struct IntegerUnboxer;

template <size_t I>
struct IntegerUnboxer<I, sizeof(int8_t)>
{
    static int8_t unboxSigned(Runtime::Reference<Runtime::IntObject> value, const std::string &name)
    {
        int64_t val = value->toInt();
        return ((val >= INT8_MIN) && (val <= INT8_MAX)) ? static_cast<int8_t>(val) : throw ValueCheckFailed<I>(value, name);
    }

    static uint8_t unboxUnsigned(Runtime::Reference<Runtime::IntObject> value, const std::string &name)
    {
        uint64_t val = value->toUInt();
        return (val <= UINT8_MAX) ? static_cast<uint8_t>(val) : throw ValueCheckFailed<I>(value, name);
    }
};

template <size_t I>
struct IntegerUnboxer<I, sizeof(int16_t)>
{
    static int16_t unboxSigned(Runtime::Reference<Runtime::IntObject> value, const std::string &name)
    {
        int64_t val = value->toInt();
        return ((val >= INT16_MIN) && (val <= INT16_MAX)) ? static_cast<int16_t>(val) : throw ValueCheckFailed<I>(value, name);
    }

    static uint16_t unboxUnsigned(Runtime::Reference<Runtime::IntObject> value, const std::string &name)
    {
        uint64_t val = value->toUInt();
        return (val <= UINT16_MAX) ? static_cast<uint16_t>(val) : throw ValueCheckFailed<I>(value, name);
    }
};

template <size_t I>
struct IntegerUnboxer<I, sizeof(int32_t)>
{
    static int32_t unboxSigned(Runtime::Reference<Runtime::IntObject> value, const std::string &name)
    {
        int64_t val = value->toInt();
        return ((val >= INT32_MIN) && (val <= INT32_MAX)) ? static_cast<int32_t>(val) : throw ValueCheckFailed<I>(value, name);
    }

    static uint32_t unboxUnsigned(Runtime::Reference<Runtime::IntObject> value, const std::string &name)
    {
        uint64_t val = value->toUInt();
        return (val <= UINT32_MAX) ? static_cast<uint32_t>(val) : throw ValueCheckFailed<I>(value, name);
    }
};

template <size_t I>
struct IntegerUnboxer<I, sizeof(int64_t)>
{
    static int64_t  unboxSigned  (Runtime::Reference<Runtime::IntObject> value, const std::string &name) { return value->toInt();  }
    static uint64_t unboxUnsigned(Runtime::Reference<Runtime::IntObject> value, const std::string &name) { return value->toUInt(); }
};

template <size_t I, bool Signed, bool Unsigned, typename T>
struct UnboxerHelper
{
    static T unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* types that not recognized, try construct from object ref */
        return T(value);
    }
};

template <size_t I, typename T>
struct UnboxerHelper<I, false, false, Runtime::Reference<T>>
{
    static Runtime::Reference<T> unbox(Runtime::ObjectRef value, const std::string &name)
    {
        try
        {
            /* some kind of reference, try cast to that type */
            return value.as<T>();
        }
        catch (const std::bad_cast &)
        {
            /* not convertible */
            throw Runtime::Exceptions::TypeError(Utils::Strings::format(
                "Argument at position %zu%s cannot be a \"%s\" object",
                I,
                name.empty() ? "" : Utils::Strings::format("(%s)", name),
                value->type()->name()
            ));
        }
    }
};

template <size_t I, typename T>
struct UnboxerHelper<I, true, false, T>
{
    static T unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* object type check */
        if (value->isNotInstanceOf(Runtime::IntTypeObject))
            throw TypeCheckFailed<I>(Runtime::IntTypeObject, name, value);

        /* convert to int object */
        auto integer = value.as<Runtime::IntObject>();

        /* check for integer sign */
        if (!(integer->isSafeInt()))
            throw ValueCheckFailed<I>(integer, name);

        /* unbox the integer */
        return IntegerUnboxer<I, sizeof(T)>::unboxSigned(std::move(integer), name);
    }
};

template <size_t I, typename T>
struct UnboxerHelper<I, false, true, T>
{
    static T unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* object type check */
        if (value->isNotInstanceOf(Runtime::IntTypeObject))
            throw TypeCheckFailed<I>(Runtime::IntTypeObject, name, value);

        /* convert to int object */
        auto integer = value.as<Runtime::IntObject>();

        /* check for integer sign */
        if (!(integer->isSafeUInt()))
            throw ValueCheckFailed<I>(integer, name);

        /* unbox the integer */
        return IntegerUnboxer<I, sizeof(T)>::unboxUnsigned(std::move(integer), name);
    }
};

/* generic unboxer */
template <size_t I, typename T>
struct Unboxer
{
    static std::decay_t<T> unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* doesn't support pointers, R-value references or mutable references */
        static_assert(!(std::is_pointer_v<T>), "Pointers are not supported");
        static_assert(!(std::is_rvalue_reference_v<T>), "R-value references are not supported");
        static_assert(!(IsMutableLValueReference<T>::value), "Mutable references are not supported");

        /* box the value */
        return UnboxerHelper<
            I,
            std::is_signed_v<std::decay_t<T>>,      /* signed-integer check */
            std::is_unsigned_v<std::decay_t<T>>,    /* unsigned-integer check */
            std::decay_t<T>
        >::unbox(value, name);
    }
};

/* reference unboxer */
template <size_t I, typename T>
struct Unboxer<I, const T &>
{
    static std::decay_t<T> unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* call the reference-removed version */
        return Unboxer<I, std::decay_t<T>>::unbox(std::move(value), name);
    }
};

/* boolean unboxer */
template <size_t I>
struct Unboxer<I, bool>
{
    static bool unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* convert to boolean */
        return value->isTrue();
    }
};

/* single precision floating point number unboxer */
template <size_t I>
struct Unboxer<I, float>
{
    static float unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* object type check */
        if (value->isNotInstanceOf(Runtime::DecimalTypeObject))
            throw TypeCheckFailed<I>(Runtime::DecimalTypeObject, name, value);

        /* convert to decimal object */
        auto decimal = value.as<Runtime::DecimalObject>();

        /* check for value range */
        if (!(decimal->isSafeFloat()))
            throw ValueCheckFailed<I>(decimal, name);

        /* convert to single-precision floating poing number */
        return decimal->toFloat();
    }
};

/* double precision floating point number unboxer */
template <size_t I>
struct Unboxer<I, double>
{
    static double unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* object type check */
        if (value->isNotInstanceOf(Runtime::DecimalTypeObject))
            throw TypeCheckFailed<I>(Runtime::DecimalTypeObject, name, value);

        /* convert to decimal object */
        auto decimal = value.as<Runtime::DecimalObject>();

        /* check for value range */
        if (!(decimal->isSafeDouble()))
            throw ValueCheckFailed<I>(decimal, name);

        /* convert to double-precision floating poing number */
        return decimal->toDouble();
    }
};

/* extended precision floating point number unboxer */
template <size_t I>
struct Unboxer<I, long double>
{
    static long double unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* object type check */
        if (value->isNotInstanceOf(Runtime::DecimalTypeObject))
            throw TypeCheckFailed<I>(Runtime::DecimalTypeObject, name, value);

        /* convert to decimal object */
        auto decimal = value.as<Runtime::DecimalObject>();

        /* check for value range */
        if (!(decimal->isSafeLongDouble()))
            throw ValueCheckFailed<I>(decimal, name);

        /* convert to extended-precision floating poing number */
        return decimal->toLongDouble();
    }
};

/* direct object */
template <size_t I>
struct Unboxer<I, Runtime::ObjectRef>
{
    static Runtime::ObjectRef unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* just return AS IS */
        return value;
    }
};

/* STL strings */
template <size_t I>
struct Unboxer<I, std::string>
{
    static std::string unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* object type checking */
        if (value->isNotInstanceOf(Runtime::StringTypeObject))
            throw TypeCheckFailed<I>(Runtime::StringTypeObject, name, value);
        else
            return value.as<Runtime::StringObject>()->value();
    }
};

/* high-precision decimal */
template <size_t I>
struct Unboxer<I, Utils::Decimal>
{
    static Utils::Decimal unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* object type checking */
        if (value->isNotInstanceOf(Runtime::DecimalTypeObject))
            throw TypeCheckFailed<I>(Runtime::DecimalTypeObject, name, value);
        else
            return value.as<Runtime::DecimalObject>()->value();
    }
};

/* high-precision integer */
template <size_t I>
struct Unboxer<I, Utils::Integer>
{
    static Utils::Integer unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* object type checking */
        if (value->isNotInstanceOf(Runtime::IntTypeObject))
            throw TypeCheckFailed<I>(Runtime::IntTypeObject, name, value);
        else
            return value.as<Runtime::IntObject>()->value();
    }
};

/* STL vector (arrays) */
template <size_t I, typename Item>
struct Unboxer<I, std::vector<Item>>
{
    static std::vector<Item> unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* result vector */
        size_t size;
        std::vector<Item> result;
        Runtime::ObjectRef *items;

        /* array or tuple */
        if (value->isInstanceOf(Runtime::ArrayTypeObject) || value->isInstanceOf(Runtime::TupleTypeObject))
        {
            /* get it's size and item array */
            if (value->isInstanceOf(Runtime::TupleTypeObject))
            {
                size = value.as<Runtime::TupleObject>()->size();
                items = value.as<Runtime::TupleObject>()->items();
            }
            else
            {
                size = value.as<Runtime::ArrayObject>()->size();
                items = value.as<Runtime::ArrayObject>()->items().data();
            }

            /* unbox each item */
            for (size_t i = 0; i < size; i++)
            {
                result.emplace_back(Unboxer<I, Item>::unbox(
                    items[i],
                    Utils::Strings::format("%s[%zu]", name, i)
                ));
            }
        }

        /* generic iterable object */
        else
        {
            /* convert to iterator */
            size_t i = 0;
            Runtime::ObjectRef iter = value->type()->iterableIter(value);

            /* iterate through each item */
            try
            {
                while (true)
                {
                    result.emplace_back(Unboxer<I, Item>::unbox(
                        iter->type()->iterableNext(iter),
                        Utils::Strings::format("%s[%zu]", name, i++)
                    ));
                }
            }

            /* this exception is expected */
            catch (const Runtime::Exceptions::StopIteration &)
            {
                /* `StopIteration` is expected, we use this
                 * to identify iteration is over, so ignore it */
            }
        }

        /* move to prevent copy */
        return std::move(result);
    }
};

/* generic map unboxer */
template <size_t I, typename Map, typename Key, typename Value>
struct MapUnboxer
{
    static Map unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* result map */
        Map result;

        /* map object */
        if (value->isInstanceOf(Runtime::MapTypeObject))
        {
            value.as<Runtime::MapObject>()->enumerateCopy([&](Runtime::ObjectRef key, Runtime::ObjectRef value)
            {
                /* add to result */
                result.emplace(
                    Unboxer<I, Key  >::unbox(key  , Utils::Strings::format("%s[key]"  , name)),
                    Unboxer<I, Value>::unbox(value, Utils::Strings::format("%s[value]", name))
                );

                /* continue enumerating */
                return true;
            });
        }

        /* generic iterable object */
        else
        {
            /* convert to iterator */
            size_t i = 0;
            Runtime::ObjectRef iter = value->type()->iterableIter(value);

            /* iterate through each item */
            try
            {
                while (true)
                {
                    /* get next pair */
                    Runtime::ObjectRef item = iter->type()->iterableNext(iter);

                    /* must be a pair of <key, value> */
                    if (item->isNotInstanceOf(Runtime::TupleTypeObject))
                    {
                        throw Runtime::Exceptions::TypeError(Utils::Strings::format(
                            "Argument at position %zu(%s[%zu]) must be a \"tuple\" object",
                            I,
                            name,
                            i
                        ));
                    }

                    /* convert to tuple */
                    Runtime::Reference<Runtime::TupleObject> tuple = item.as<Runtime::TupleObject>();

                    /* must have exact 2 items */
                    if (tuple->size() != 2)
                    {
                        throw Runtime::Exceptions::TypeError(Utils::Strings::format(
                            "Argument at position %zu(%s[%zu]) must be a \"tuple\" object of two elements",
                            I,
                            name,
                            i
                        ));
                    }

                    /* add to result map */
                    result.emplace(
                        Unboxer<I, Key  >::unbox(tuple->items()[0], Utils::Strings::format("%s[%zu][key]"  , i, name)),
                        Unboxer<I, Value>::unbox(tuple->items()[1], Utils::Strings::format("%s[%zu][value]", i, name))
                    );

                    /* increase item counter */
                    i++;
                }
            }

            /* this exception is expected */
            catch (const Runtime::Exceptions::StopIteration &)
            {
                /* `StopIteration` is expected, we use this
                 * to identify iteration is over, so ignore it */
            }
        }

        /* move to prevent copy */
        return std::move(result);
    }
};

/* STL map (tree map) */
template <size_t I, typename Key, typename Value>
struct Unboxer<I, std::map<Key, Value>>
{
    static std::map<Key, Value> unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* call the map unboxer, tree map */
        return MapUnboxer<I, std::map<Key, Value>, Key, Value>::unbox(std::move(value), name);
    }
};

/* STL unordered map (hash map) */
template <size_t I, typename Key, typename Value>
struct Unboxer<I, std::unordered_map<Key, Value>>
{
    static std::unordered_map<Key, Value> unbox(Runtime::ObjectRef value, const std::string &name)
    {
        /* call the map unboxer, hash map */
        return MapUnboxer<I, std::unordered_map<Key, Value>, Key, Value>::unbox(std::move(value), name);
    }
};

/** Argument Pack Unboxer **/

template <size_t I, typename ... Args>
struct ArgumentPackUnboxer;

template <size_t I, typename T, typename ... Args>
struct ArgumentPackUnboxer<I, T, Args ...>
{
    static std::tuple<T, Args ...> unbox(
        VariadicArgs        &args,
        KeywordArgs         &kwargs,
        const KeywordNames  &keywords,
        const DefaultValues &defaults)
    {
        /* find keyword arguments */
        const std::string &name = keywords[I];
        Runtime:: ObjectRef value = nullptr;

        /* pop from for keyword arguments if any */
        if (name.size())
            value = kwargs->pop(Runtime::StringObject::fromStringInterned(name));

        /* keyword arguments have higher priority */
        if (value.isNull())
        {
            /* check for positional arguments */
            if (I < args->size())
                value = args->items()[I];

            /* check for default values */
            else if (I >= keywords.size() - defaults.size())
                value = defaults[I + defaults.size() - keywords.size()];

            /* oterwise it's an error */
            else
            {
                throw Runtime::Exceptions::TypeError(Utils::Strings::format(
                    "Missing required argument at position %zu%s",
                    I,
                    name.empty() ? "" : Utils::Strings::format("(%s)", name)
                ));
            }
        }

        /* build the rest of arguments */
        return std::tuple_cat(
            std::forward_as_tuple(Unboxer<I, T>::unbox(std::move(value), name)),
            ArgumentPackUnboxer<I + 1, Args ...>::unbox(args, kwargs, keywords, defaults)
        );
    }
};

template <size_t I>
struct ArgumentPackUnboxer<I>
{
    static std::tuple<> unbox(
        VariadicArgs        &args,
        KeywordArgs         &kwargs,
        const KeywordNames  &keywords,
        const DefaultValues &defaults)
    {
        /* should have no keyword arguments left */
        if (kwargs->size())
        {
            throw Runtime::Exceptions::TypeError(Utils::Strings::format(
                "Function does not accept keyword argument \"%s\"",
                kwargs->front()->type()->objectStr(kwargs->front())
            ));
        }

        /* also should have no positional arguments left */
        if (I < args->size())
        {
            throw Runtime::Exceptions::TypeError(Utils::Strings::format(
                "Function takes %s %zu argument(s), but %zu given",
                defaults.empty() ? "exact" : "at most",
                keywords.size(),
                args->size()
            ));
        }

        /* final recursion, no arguments left */
        return std::tuple<>();
    }
};

template<typename F, typename T, typename R, typename ... Args>
constexpr auto makeFunction(F &&f, R (T::*)(Args ...) const)
{
    /* wrap the object by `std::function` */
    return std::function<R(Args ...)>(f);
}

template <typename Class, typename Tuple, size_t ... I>
constexpr auto invokeConstructor(Tuple &&args, std::index_sequence<I ...>)
{
    /* invoke the constructor */
    return Class(std::get<I>(args) ...);
}
}

/** Object Boxer and Unboxer **/

template <typename T>
using Boxer = Details::Boxer<T>;

template <size_t I, typename T>
using Unboxer = Details::Unboxer<I, T>;

template <typename ... Args>
using ArgsUnboxer = Details::ArgumentPackUnboxer<0, std::decay_t<Args> ...>;

/** Meta Function **/

template <typename Ret, typename ... Args>
struct MetaFunction
{
    static Runtime::ObjectRef invoke(
        const std::function<Ret(Args ...)>  &func,
        VariadicArgs                        &args,
        KeywordArgs                         &kwargs,
        const KeywordNames                  &keywords,
        const DefaultValues                 &defaults)
    {
        /* unbox the parameter pack, invoke the function, and box the result */
        return Boxer<Ret>::box(std::apply(func, ArgsUnboxer<Args ...>::unbox(args, kwargs, keywords, defaults)));
    };
};

template <typename ... Args>
struct MetaFunction<void, Args ...>
{
    static Runtime::ObjectRef invoke(
        const std::function<void(Args ...)> &func,
        VariadicArgs                        &args,
        KeywordArgs                         &kwargs,
        const KeywordNames                  &keywords,
        const DefaultValues                 &defaults)
    {
        /* void function, return a null object */
        std::apply(func, ArgsUnboxer<Args ...>::unbox(args, kwargs, keywords, defaults));
        return Runtime::NullObject;
    };
};

/** Meta Constructor **/

template <typename T, typename ... Args>
struct MetaConstructor
{
    static Runtime::Reference<T> construct(
        VariadicArgs        &args,
        KeywordArgs         &kwargs,
        const KeywordNames  &keywords,
        const DefaultValues &defaults)
    {
        return std::apply(
            Runtime::Reference<T>::template newObject<Args ...>,
            ArgsUnboxer<Args ...>::unbox(args, kwargs, keywords, defaults)
        );
    }
};

template <typename Class, typename ... Args>
constexpr auto construct(std::tuple<Args ...> &&args)
{
    /* forward to implementation */
    return Details::invokeConstructor<Class>(
        std::move(args),
        std::index_sequence_for<Args ...>()
    );
}

template <typename Function>
constexpr auto forwardAsFunction(Function &&f)
{
    /* extract it's function signature from `Function::operator()` */
    typedef std::decay_t<Function> FunctionType;
    return Details::makeFunction(std::forward<Function>(f), &FunctionType::operator());
}

template <typename R, typename ... Args>
constexpr auto forwardAsFunction(R (*f)(Args ...))
{
    /* wrap the function pointer by `std::function` directly */
    return std::function<R(Args ...)>(f);
};
}

#endif /* REDSCRIPT_UTILS_NFI_H */
