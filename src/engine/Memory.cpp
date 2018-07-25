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
};
}

/*** Tagged Memory Allocator ***/

static inline size_t freeTag(MemoryTag *tag) __attribute__((always_inline));
static inline size_t freeTag(MemoryTag *tag)
{
    size_t size = tag->size;
    free(reinterpret_cast<void *>(tag));
    return size;
}

static inline MemoryTag *allocTag(size_t size) __attribute__((always_inline));
static inline MemoryTag *allocTag(size_t size)
{
    /* allocate memory */
    size_t alloc = size + sizeof(MemoryTag);
    MemoryTag *tag = reinterpret_cast<MemoryTag *>(malloc(alloc));

    /* initialize memory tag */
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
        __sync_sub_and_fetch(&_objectUsage, freeTag(reinterpret_cast<MemoryTag *>(ptr) - 1));
    }
}

void *Memory::alloc(size_t size)
{
    /* allocate memory with tag */
    MemoryTag *tag = allocTag(size);

    /* update object counter and usage */
    __sync_add_and_fetch(&_objectCount, 1);
    __sync_add_and_fetch(&_objectUsage, tag->size);

    /* skip tag header */
    return reinterpret_cast<void *>(tag + 1);
}
}

/*** System `new` and `delete` monitor ***/

void *operator new(size_t size)
{
    /* allocate memory with tag */
    MemoryTag *tag = allocTag(size);

    /* update object counter and usage */
    __sync_add_and_fetch(&_rawCount, 1);
    __sync_add_and_fetch(&_rawUsage, tag->size);

    /* skip tag header */
    return reinterpret_cast<void *>(tag + 1);
}

void *operator new[](size_t size)
{
    /* allocate memory with tag */
    MemoryTag *tag = allocTag(size);

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
        __sync_sub_and_fetch(&_rawUsage, freeTag(reinterpret_cast<MemoryTag *>(ptr) - 1));
    }
}

void operator delete[](void *ptr) noexcept
{
    if (ptr != nullptr)
    {
        __sync_sub_and_fetch(&_arrayCount, 1);
        __sync_sub_and_fetch(&_arrayUsage, freeTag(reinterpret_cast<MemoryTag *>(ptr) - 1));
    }
}
