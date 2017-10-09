#ifndef REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H
#define REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H

#include <atomic>
#include <cstdio>
#include <cstdint>

namespace RedScript::Engine
{
class GCObject
{
    GCObject *_prev = nullptr;
    GCObject *_next = nullptr;

private:
    size_t  _size;
    int32_t _level;
    std::atomic_int32_t _refCount;

private:
    friend class GarbageCollector;
    static_assert(std::atomic_int32_t::is_always_lock_free, "Non lock-free reference counter");

public:
    void add(void);
    void remove(void);

};

struct GarbageCollector
{
    static int gc(void);
    static void freeObject(void *obj);
    static void *allocObject(size_t size);
};
}

#endif /* REDSCRIPT_ENGINE_GARBAGECOLLECTOR_H */
