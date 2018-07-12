#ifndef REDSCRIPT_UTILS_LISTS_H
#define REDSCRIPT_UTILS_LISTS_H

#include <vector>
#include <cstdint>
#include <utility>

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
}

#endif /* REDSCRIPT_UTILS_LISTS_H */
