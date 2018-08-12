#ifndef REDSCRIPT_ENGINE_MEMORY_H
#define REDSCRIPT_ENGINE_MEMORY_H

#include <cstdlib>
#include <utility>
#include <type_traits>

namespace RedScript::Engine
{
struct Memory
{
    static inline void free(void *ptr) { ::free(ptr); }
    static inline void *alloc(size_t size) { return ::malloc(size ? (((size - 1) >> 4) + 1) << 4 : 0); }
    static inline void *realloc(void *ptr, size_t size) { return ::realloc(ptr, size ? (((size - 1) >> 4) + 1) << 4 : 0); }

public:
    template <typename T>
    static inline T *alloc(size_t count = 1)
    {
        return reinterpret_cast<T *>(alloc(sizeof(T) * count));
        static_assert(std::is_trivial_v<T>, "Not a trivial type");
    }

/*** Object Construction and Destruction ***/

public:
    template <typename Object>
    static inline void destroy(Object *self) noexcept
    {
        /* call the destructor directly */
        if (self) self->~Object();
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
