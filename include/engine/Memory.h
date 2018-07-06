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
    static constexpr int MEM_RAW    = 1;
    static constexpr int MEM_ARRAY  = 2;
    static constexpr int MEM_OBJECT = 3;

public:
    static size_t rawCount(void);
    static size_t arrayCount(void);
    static size_t objectCount(void);

public:
    static size_t rawUsage(void);
    static size_t arrayUsage(void);
    static size_t objectUsage(void);

public:
    static void free(void *ptr);
    static void *alloc(size_t size);

public:
    static int typeOf(void *ptr);
    static size_t sizeOf(void *ptr);

/*** Object Construction and Destruction ***/

public:
    template <typename Object>
    static inline void destroy(Object *self) noexcept
    {
        /* call the destructor directly */
        if (self) self->~T();
    }

public:
    template <typename Object, typename ... Args>
    static inline Object *construct(void *self, Args &&... args)
    {
        /* use placement-new to construct the object */
        return new (self) Object(std::forward<Args>(args) ...);
    }
};
}

#endif /* REDSCRIPT_ENGINE_MEMORY_H */
