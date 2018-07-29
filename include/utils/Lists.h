#ifndef REDSCRIPT_UTILS_LISTS_H
#define REDSCRIPT_UTILS_LISTS_H

#include <vector>
#include <cstdint>
#include <utility>

#include "utils/Strings.h"
#include "runtime/Object.h"
#include "runtime/IntObject.h"
#include "runtime/ExceptionObject.h"

namespace RedScript::Utils::Lists
{
namespace Details
{
template <size_t, typename Item> static inline void fill(Item *items) {}
template <size_t, typename Item> static inline void fill(std::vector<Item> &items) {}

template <size_t I, typename Item, typename ... Args>
static inline void fill(Item *items, Item first, Args && ... remains)
{
    items[I] = first;
    fill<I + 1, Item>(items, std::forward<Args>(remains) ...);
}

template <size_t I, typename Item, typename ... Args>
static inline void fill(std::vector<Item> &items, Item first, Args && ... remains)
{
    items[I] = first;
    fill<I + 1, Item>(items, std::forward<Args>(remains) ...);
}
}

template <typename Item, typename ... Args>
static inline void fill(Item *items, Args && ... args)
{
    /* call the actual fill function */
    return Details::fill<0, Item>(items, std::forward<Args>(args) ...);
}

template <typename Item, typename ... Args>
static inline void fill(std::vector<Item> &items, Args && ... args)
{
    /* call the actual fill function */
    return Details::fill<0, Item>(items, std::forward<Args>(args) ...);
}

template <typename List>
static inline bool contains(Runtime::Reference<List> list, Runtime::ObjectRef value)
{
    /* check for each item */
    for (size_t i = 0; i < list->size(); i++)
    {
        /* extract the item */
        Runtime::ObjectRef item = list->items()[i];

        /* must not be null */
        if (item.isNull())
            throw Runtime::Exceptions::InternalError("Array contains null value");

        /* check for equality */
        if (item == value)
            return true;
    }

    /* not found */
    return false;
}

template <typename List>
static inline size_t indexConstraint(Runtime::Reference<List> list, Runtime::ObjectRef value)
{
    /* integer type check */
    if (value->isNotInstanceOf(Runtime::IntTypeObject))
    {
        throw Runtime::Exceptions::TypeError(Strings::format(
            "Index must be integers, not \"%s\"",
            value->type()->name()
        ));
    }

    /* convert to integers */
    size_t pos;
    size_t size = list->size();
    Runtime::Reference<Runtime::IntObject> index = value.as<Runtime::IntObject>();

    /* it's an unsigned integer */
    if (index->isSafeUInt())
    {
        /* counting from front */
        if ((pos = index->toUInt()) < size)
            return pos;

        /* otherwise it's an error */
        throw Runtime::Exceptions::IndexError(Strings::format(
            "Index out of range [-%lu, %lu): %zu",
            size,
            size,
            pos
        ));
    }

    /* it's a negative signed integer, but it's
     * absolute value can fit into an unsigned integer */
    if (index->isSafeNegativeUInt())
    {
        /* counting from back */
        if ((pos = index->toNegativeUInt()) <= size)
            return size - pos;

        /* otherwise it's an error */
        throw Runtime::Exceptions::IndexError(Strings::format(
            "Index out of range [-%lu, %lu): -%zu",
            size,
            size,
            pos
        ));
    }

    /* otherwise it's an error */
    throw Runtime::Exceptions::IndexError(Strings::format(
        "Index out of range [-%lu, %lu): %s",
        size,
        size,
        index->value().toString()
    ));
}

struct Slice
{
    size_t begin;
    size_t count;
    ssize_t step;
};

template <typename List>
static inline Slice sliceConstraint(
    Runtime::Reference<List> list,
    Runtime::ObjectRef       begin,
    Runtime::ObjectRef       end,
    Runtime::ObjectRef       step)
{
    /* empty list */
    if (list->size() == 0)
        return Slice { 0, 0, 0 };

    /* default: from begin to end, through every element */
    size_t stop = list->size() - 1;
    size_t start = 0;
    ssize_t stride = 1;

    /* have stepping expression */
    if (step.isNotNull())
    {
        /* must be an integer */
        if (step->isNotInstanceOf(Runtime::IntTypeObject))
        {
            throw Runtime::Exceptions::TypeError(Strings::format(
                "Slice step must be an integer, not \"%s\"",
                step->type()->name()
            ));
        }

        /* convert to integer */
        auto obj = step.as<Runtime::IntObject>();
        const auto &value = obj->value();

        /* too large to cover even two elements */
        if (!(value.isSafeInt()))
            return Slice { start, 1, 1 };

        /* convert to signed integer */
        if (!(stride = value.toInt()))
            throw Runtime::Exceptions::ValueError("Slice step cannot be zero");

        /* backward slicing */
        if (stride < 0)
        {
            stop = 0;
            start = list->size() - 1;
        }
    }

    /* have ending expression, convert to
     * unsigned size type with index constraint */
    if (end.isNotNull())
        stop = indexConstraint(list, end);

    /* have beginning expression, convert to
     * unsigned size type with index constraint */
    if (begin.isNotNull())
        start = indexConstraint(list, begin);

    /* might be empty slice */
    if (((stride > 0) && (stop < start)) ||
        ((stride < 0) && (stop > start)))
        return Slice { 0, 0, 0 };

    /* create slicing object that corresponds to slicing direction */
    if (stride > 0)
        return Slice { start, (stop - start) / stride + 1, stride };
    else
        return Slice { start, (start - stop) / -stride + 1, stride };
}

template <typename List>
static inline int64_t totalOrderCompare(
    Runtime::Reference<List> a,
    Runtime::Reference<List> b)
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
        throw Runtime::Exceptions::InternalError("\"__compare__\" gives null");

    /* must be integers */
    if (ret->isNotInstanceOf(Runtime::IntTypeObject))
    {
        throw Runtime::Exceptions::TypeError(Strings::format(
            "\"__compare__\" must returns an integer, not \"%s\"",
            ret->type()->name()
        ));
    }

    /* convert to integer */
    Runtime::Reference<Runtime::IntObject> val = ret.as<Runtime::IntObject>();

    /* must be a signed integer */
    if (!(val->isSafeInt()))
        throw Runtime::Exceptions::ValueError("\"__compare__\" must returns a valid signed integer");

    /* convert to integer */
    return val->toInt();
}
}

#endif /* REDSCRIPT_UTILS_LISTS_H */
