#include <new>
#include <atomic>
#include <cstdint>
#include <cstdlib>

#include "engine/Memory.h"

#define MEM_ALIGN       16
#define MEM_ALIGN_MASK  0x0f

namespace
{
struct MemoryTag
{
    uint64_t size;
    uint64_t type;
};
}

/* check for memory tag alignment */
static_assert(sizeof(MemoryTag) == MEM_ALIGN, "Unaligned memory tag");

/*** Aligned Memory Allocator ***/

static void freeAligned(void *ptr)
{
    /* simply call standard free */
    std::free(ptr);
}

static void *allocAligned(size_t size)
{
    /* allocate memory with alignment `MEM_ALIGN` */
    void *mem;
    return posix_memalign(&mem, MEM_ALIGN, size) ? throw std::bad_alloc() : mem;
}

/*** Tagged Memory Allocator ***/

static size_t freeTag(uint64_t type, MemoryTag *tag)
{
    /* get the address */
    size_t size;
    uintptr_t addr = reinterpret_cast<uintptr_t>(tag);

    /* validate pointer */
    if ((addr & MEM_ALIGN_MASK))
    {
        fprintf(stderr, "*** FATAL: invalid free address %p\n", tag);
        abort();
    }

    /* check for object type */
    if (tag->type != type)
    {
        fprintf(stderr, "*** FATAL: memory type mismatch on blck %p: %lu -> %lu\n", tag, type, tag->type);
        abort();
    }

    /* extract the size, and release the memory tag */
    size = tag->size;
    freeAligned(reinterpret_cast<void *>(tag));
    return size;
}

static MemoryTag *allocTag(uint64_t type, size_t size)
{
    /* allocate memory */
    size_t alloc = size + MEM_ALIGN;
    MemoryTag *tag = reinterpret_cast<MemoryTag *>(allocAligned(alloc));

    /* initialize memory tag */
    tag->type = type;
    tag->size = alloc;
    return tag;
}

/* wrap counters in static functions to prevent initializing order problem
 * compilers would optimize them away, thus it's not really a problem */

static inline std::atomic_size_t &_rawUsage(void)    { static std::atomic_size_t value(0); return value; }
static inline std::atomic_size_t &_arrayUsage(void)  { static std::atomic_size_t value(0); return value; }
static inline std::atomic_size_t &_objectUsage(void) { static std::atomic_size_t value(0); return value; }

namespace RedScript::Engine
{
size_t Memory::rawUsage(void) { return _rawUsage().load(); }
size_t Memory::arrayUsage(void) { return _arrayUsage().load(); }
size_t Memory::objectUsage(void) { return _objectUsage().load(); }

void Memory::free(void *ptr)
{
    if (ptr != nullptr)
    {
        _objectUsage() -= freeTag(
            MEM_OBJECT,
            reinterpret_cast<MemoryTag *>(ptr) - 1
        );
    }
}

void *Memory::alloc(size_t size)
{
    MemoryTag *tag = allocTag(MEM_OBJECT, size);
    _objectUsage() += tag->size;
    return reinterpret_cast<void *>(tag + 1);
}

int Memory::typeOf(void *ptr)
{
    /* get memory type from memory tag */
    return static_cast<int>((reinterpret_cast<MemoryTag *>(ptr) - 1)->type);
}

size_t Memory::sizeOf(void *ptr)
{
    /* get size info from memory tag */
    return (reinterpret_cast<MemoryTag *>(ptr) - 1)->size;
}
}

/*** System `new` and `delete` monitor ***/

void *operator new(size_t size)
{
    MemoryTag *tag = allocTag(RedScript::Engine::Memory::MEM_RAW, size);
    _rawUsage() += tag->size;
    return reinterpret_cast<void *>(tag + 1);
}

void *operator new[](size_t size)
{
    MemoryTag *tag = allocTag(RedScript::Engine::Memory::MEM_ARRAY, size);
    _arrayUsage() += tag->size;
    return reinterpret_cast<void *>(tag + 1);
}

void operator delete(void *ptr) noexcept
{
    if (ptr != nullptr)
    {
        _rawUsage() -= freeTag(
            RedScript::Engine::Memory::MEM_RAW,
            reinterpret_cast<MemoryTag *>(ptr) - 1
        );
    }
}

void operator delete[](void *ptr) noexcept
{
    if (ptr != nullptr)
    {
        _arrayUsage() -= freeTag(
            RedScript::Engine::Memory::MEM_ARRAY,
            reinterpret_cast<MemoryTag *>(ptr) - 1
        );
    }
}
