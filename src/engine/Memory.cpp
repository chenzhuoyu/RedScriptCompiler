#include <new>
#include <atomic>
#include <cstdint>
#include <cstdlib>

#include "engine/Memory.h"

#define MEM_ALIGN       16
#define MEM_ALIGN_MASK  0x0f

#define MEM_RAW         1
#define MEM_ARRAY       2
#define MEM_OBJECT      3

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
        fprintf(stderr, "*** FATAL: memory type mismatch on blck %p: %llu -> %llu\n", tag, type, tag->type);
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

namespace RedScript::Engine
{
/* memory usage counters */
std::atomic_size_t Memory::_rawUsage;
std::atomic_size_t Memory::_arrayUsage;
std::atomic_size_t Memory::_objectUsage;

void Memory::free(void *ptr)
{
    if (ptr != nullptr)
        _objectUsage -= freeTag(MEM_OBJECT, reinterpret_cast<MemoryTag *>(ptr) - 1);
}

void *Memory::alloc(size_t size)
{
    /* allocate tagged memory */
    MemoryTag *tag = allocTag(MEM_OBJECT, size);

    /* update object usage counter */
    _objectUsage += tag->size;
    return reinterpret_cast<void *>(tag + 1);
}
}

/*** System `new` and `delete` monitor ***/

void *operator new(size_t size)
{
    MemoryTag *tag = allocTag(MEM_RAW, size);
    RedScript::Engine::Memory::_rawUsage += tag->size;
    return reinterpret_cast<void *>(tag + 1);
}

void *operator new[](size_t size)
{
    MemoryTag *tag = allocTag(MEM_ARRAY, size);
    RedScript::Engine::Memory::_arrayUsage += tag->size;
    return reinterpret_cast<void *>(tag + 1);
}

void operator delete(void *ptr) noexcept
{
    if (ptr != nullptr)
        RedScript::Engine::Memory::_rawUsage -= freeTag(MEM_RAW, reinterpret_cast<MemoryTag *>(ptr) - 1);
}

void operator delete[](void *ptr) noexcept
{
    if (ptr != nullptr)
        RedScript::Engine::Memory::_arrayUsage -= freeTag(MEM_ARRAY, reinterpret_cast<MemoryTag *>(ptr) - 1);
}
