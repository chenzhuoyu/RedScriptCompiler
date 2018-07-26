#ifndef REDSCRIPT_ENGINE_MEMORY_H
#define REDSCRIPT_ENGINE_MEMORY_H

#include <cstdlib>
#include <utility>

namespace RedScript::Engine
{
struct Memory
{
    static inline void free(void *ptr) { ::free(ptr); }
    static inline void *alloc(size_t size) { return ::malloc(size); }
    static inline void *realloc(void *ptr, size_t size) { return ::realloc(ptr, size); }

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
