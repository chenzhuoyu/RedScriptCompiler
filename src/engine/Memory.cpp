#include <new>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cinttypes>

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
        fprintf(stderr, "*** FATAL: memory type mismatch on block %p: %" PRIu64 " -> %" PRIu64 "\n", tag, type, tag->type);
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
    size_t alloc = size + sizeof(MemoryTag);
    MemoryTag *tag = reinterpret_cast<MemoryTag *>(allocAligned(alloc));

    /* initialize memory tag */
    tag->type = type;
    tag->size = alloc;
    return tag;
}

/* memory block counters */
static size_t _rawCount = 0;
static size_t _arrayCount = 0;
static size_t _objectCount = 0;

/* memory usage counters */
static size_t _rawUsage = 0;
static size_t _arrayUsage = 0;
static size_t _objectUsage = 0;

namespace RedScript::Engine
{
size_t Memory::rawCount(void)    { return _rawCount;    }
size_t Memory::arrayCount(void)  { return _arrayCount;  }
size_t Memory::objectCount(void) { return _objectCount; }

size_t Memory::rawUsage(void)    { return _rawUsage;    }
size_t Memory::arrayUsage(void)  { return _arrayUsage;  }
size_t Memory::objectUsage(void) { return _objectUsage; }

void Memory::free(void *ptr)
{
    if (ptr != nullptr)
    {
        __sync_sub_and_fetch(&_objectCount, 1);
        __sync_sub_and_fetch(&_objectUsage, freeTag(MEM_OBJECT, reinterpret_cast<MemoryTag *>(ptr) - 1));
    }
}

void *Memory::alloc(size_t size)
{
    /* allocate memory with tag */
    MemoryTag *tag = allocTag(MEM_OBJECT, size);

    /* update object counter and usage */
    __sync_add_and_fetch(&_objectCount, 1);
    __sync_add_and_fetch(&_objectUsage, tag->size);

    /* skip tag header */
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
    /* allocate memory with tag */
    MemoryTag *tag = allocTag(RedScript::Engine::Memory::MEM_RAW, size);

    /* update object counter and usage */
    __sync_add_and_fetch(&_rawCount, 1);
    __sync_add_and_fetch(&_rawUsage, tag->size);

    /* skip tag header */
    return reinterpret_cast<void *>(tag + 1);
}

void *operator new[](size_t size)
{
    /* allocate memory with tag */
    MemoryTag *tag = allocTag(RedScript::Engine::Memory::MEM_ARRAY, size);

    /* update object counter and usage */
    __sync_add_and_fetch(&_arrayCount, 1);
    __sync_add_and_fetch(&_arrayUsage, tag->size);

    /* skip tag header */
    return reinterpret_cast<void *>(tag + 1);
}

void operator delete(void *ptr) noexcept
{
    if (ptr != nullptr)
    {
        __sync_sub_and_fetch(&_rawCount, 1);
        __sync_sub_and_fetch(&_rawUsage, freeTag(RedScript::Engine::Memory::MEM_RAW, reinterpret_cast<MemoryTag *>(ptr) - 1));
    }
}

void operator delete[](void *ptr) noexcept
{
    if (ptr != nullptr)
    {
        __sync_sub_and_fetch(&_arrayCount, 1);
        __sync_sub_and_fetch(&_arrayUsage, freeTag(RedScript::Engine::Memory::MEM_ARRAY, reinterpret_cast<MemoryTag *>(ptr) - 1));
    }
}
