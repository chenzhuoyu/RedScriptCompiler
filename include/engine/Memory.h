#ifndef REDSCRIPT_ENGINE_MEMORY_H
#define REDSCRIPT_ENGINE_MEMORY_H

#include <cstdio>
#include <memory>
#include <utility>

namespace RedScript::Engine
{
class Memory
{
    friend void *::operator new(size_t);
    friend void *::operator new[](size_t);

private:
    friend void ::operator delete(void *) noexcept;
    friend void ::operator delete[](void *) noexcept;

public:
    static size_t rawUsage(void);
    static size_t arrayUsage(void);
    static size_t objectUsage(void);

public:
    static void free(void *ptr);
    static void *alloc(size_t size);

/*** Manual Object Construction and Destruction ***/

public:
    template <typename T>
    static inline void destroy(T *self) noexcept
    {
        /* call the destructor directly */
        if (self) self->~T();
    }

public:
    template <typename T, typename ... Args>
    static inline T *construct(void *self, Args &&... args)
    {
        /* use placement-new to construct the object */
        return new (self) T(std::forward<Args>(args) ...);
    }

/*** Convient Functions ***/

public:
    template <typename T>
    static inline void freeObject(T *self)
    {
        destroy(self);
        free(self);
    }

public:
    template <typename T, typename ... Args>
    static inline T *allocObject(Args &&... args)
    {
        void *mem = alloc(sizeof(T));
        return construct<T>(mem, std::forward<Args>(args) ...);
    }
};
}

#endif /* REDSCRIPT_ENGINE_MEMORY_H */
