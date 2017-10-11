#include <new>
#include <atomic>
#include <cstdlib>

#include "engine/Memory.h"

#define MEM_ALIGN       16
#define MEM_ALIGN_MASK  0x0f

namespace RedScript::Engine
{
/* memory usage counter */
static std::atomic_size_t _usage;

void Memory::free(void *ptr)
{
    /* should compatible with `nullptr` */
    if (ptr)
    {
        /* get the address */
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t realptr = addr - MEM_ALIGN;

        /* validate pointer */
        if ((addr & MEM_ALIGN_MASK))
            throw std::bad_alloc();

        /* update usage counter, and release the memory */
        _usage -= *reinterpret_cast<size_t *>(realptr);
        std::free(reinterpret_cast<void *>(realptr));
    }
}

void *Memory::alloc(size_t size)
{
    /* allocate memory, aligned with 16-bytes */
    void *ptr;
    size_t alloc = size + MEM_ALIGN;
    uintptr_t addr = reinterpret_cast<uintptr_t>(posix_memalign(&ptr, MEM_ALIGN, alloc) ? throw std::bad_alloc() : ptr);

    /* update size field */
    _usage += alloc;
    *reinterpret_cast<size_t *>(addr) = alloc;
    return reinterpret_cast<void *>(addr + MEM_ALIGN);
}

size_t Memory::usage(void)
{
    /* load from usage counter */
    return _usage.load();
}
}
