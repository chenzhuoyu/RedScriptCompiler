#ifndef REDSCRIPT_ENGINE_MEMORY_H
#define REDSCRIPT_ENGINE_MEMORY_H

#include <cstdio>
#include <utility>

namespace RedScript::Engine
{
struct Memory
{
    static void free(void *ptr);
    static void *alloc(size_t size);

public:
    static size_t usage(void);

public:
    template <typename T>
    static void destruct(T *self) noexcept
    {
        /* call the destructor directly */
        self->~T();
    }

public:
    template <typename T, typename ... Args>
    static T *construct(void *self, Args &&... args)
    {
        /* use placement-new to construct the object */
        return new (self) T(std::forward<Args>(args) ...);
    }
};
}

#endif /* REDSCRIPT_ENGINE_MEMORY_H */
