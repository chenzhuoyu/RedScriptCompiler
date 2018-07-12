#ifndef REDSCRIPT_UTILS_LISTS_H
#define REDSCRIPT_UTILS_LISTS_H

#include <vector>
#include <cstdint>
#include <utility>

#include "utils/Strings.h"
#include "runtime/Object.h"
#include "runtime/IntObject.h"
#include "exceptions/TypeError.h"
#include "exceptions/IndexError.h"
#include "exceptions/InternalError.h"

namespace RedScript::Utils::Lists
{
namespace Details
{
template <size_t, typename T> static inline void fill(T *items) {}
template <size_t, typename T> static inline void fill(std::vector<T> &items) {}

template <size_t I, typename T, typename ... Args>
static inline void fill(T *items, T first, Args && ... remains)
{
    items[I] = first;
    fill<I + 1, T>(items, std::forward<Args>(remains) ...);
}

template <size_t I, typename T, typename ... Args>
static inline void fill(std::vector<T> &items, T first, Args && ... remains)
{
    items[I] = first;
    fill<I + 1, T>(items, std::forward<Args>(remains) ...);
}
}

template <typename T, typename ... Args>
static inline void fill(T *items, Args && ... args)
{
    /* call the actual fill function */
    return Details::fill<0, T>(items, std::forward<Args>(args) ...);
}

template <typename T, typename ... Args>
static inline void fill(std::vector<T> &items, Args && ... args)
{
    /* call the actual fill function */
    return Details::fill<0, T>(items, std::forward<Args>(args) ...);
}

template <typename T>
static inline bool contains(Runtime::Reference<T> list, Runtime::ObjectRef value)
{
    /* check for each item */
    for (size_t i = 0; i < list->size(); i++)
    {
        /* extract the item */
        Runtime::ObjectRef item = list->items()[i];

        /* must not be null */
        if (item.isNull())
            throw Exceptions::InternalError("Array contains null value");

        /* check for equality */
        if (item == value)
            return true;
    }

    /* not found */
    return false;
}

template <typename T>
static inline size_t indexConstraint(Runtime::Reference<T> list, Runtime::Reference<Runtime::IntObject> index)
{
    /* positive 64-bit integer */
    if (index->isSafeUInt())
    {
        /* counting from front */
        size_t n = list->size();
        size_t v = index->toUInt();

        /* extract the index as unsigned integer */
        if (v >= n)
            throw Exceptions::IndexError(Strings::format("Index out of range [-%lu, %lu): %zu", n, n, v));
        else
            return v;
    }

    /* negative 64-bit integer */
    else if (index->isSafeNegativeUInt())
    {
        /* counting from back */
        size_t n = list->size();
        size_t v = index->toNegativeUInt();

        /* check the index */
        if (n < v)
            throw Exceptions::IndexError(Strings::format("Index out of range [-%lu, %lu): -%zu", n, n, v));
        else
            return n - v;
    }

    /* other number, definately out of range */
    else
    {
        size_t n = list->size();
        std::string v = index->value().toString();
        throw Exceptions::IndexError(Strings::format("Index out of range [-%lu, %lu): %s", n, n, v));
    }
}

template <typename T>
static inline int64_t totalOrderCompare(Runtime::Reference<T> a, Runtime::Reference<T> b)
{
    /* tuple lengths */
    size_t i = 0;
    size_t alen = a->size();
    size_t blen = b->size();

    /* find the first item that not equals */
    for (; (i < alen) && (i < blen); i++)
        if (!(a->items()[i] == b->items()[i]))
            break;

    /* all items are equal, compare sizes */
    if ((i >= alen) || (i >= blen))
        return (alen == blen) ? 0 : (alen > blen) ? 1 : -1;

    /* use total order comparison to determain the last item */
    Runtime::ObjectRef v = a->items()[i];
    Runtime::ObjectRef w = b->items()[i];
    Runtime::ObjectRef ret = v->type()->comparableCompare(v, w);

    /* cannot be null */
    if (ret.isNull())
        throw Exceptions::InternalError("\"__compare__\" gives null");

    /* must be integers */
    if (ret->isNotInstanceOf(Runtime::IntTypeObject))
    {
        throw Exceptions::TypeError(Strings::format(
            "\"__compare__\" must returns an integer, not \"%s\"",
            ret->type()->name()
        ));
    }

    /* convert to integer */
    Runtime::Reference<Runtime::IntObject> val = ret.as<Runtime::IntObject>();

    /* must be a signed integer */
    if (!(val->isSafeInt()))
        throw Exceptions::ValueError("\"__compare__\" must returns a valid signed integer");

    /* convert to integer */
    return val->toInt();
}
}

#endif /* REDSCRIPT_UTILS_LISTS_H */
