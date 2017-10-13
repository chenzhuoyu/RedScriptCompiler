#ifndef REDSCRIPT_ENGINE_THREAD_H
#define REDSCRIPT_ENGINE_THREAD_H

#include <vector>
#include <cstdint>
#include <utility>
#include <functional>

namespace RedScript::Engine
{
class Thread final
{
    struct Storage
    {
        void *ptr;
        size_t reclaim;
        std::function<void(void *)> destroy;
    };

public:
    static constexpr size_t MIN_LOCALS  = 64;
    static constexpr size_t MAX_LOCALS  = 65536;
    static constexpr size_t MAX_THREADS = 65536;

private:
    Thread();
   ~Thread();

private:
    int _key;
    std::vector<Storage *> _locals;

public:
    int key(void) const { return _key; }

public:
    static size_t count(void);
    static Thread &current(void);

private:
    static int allocLocalId(size_t &reclaim);
    static void releaseLocalId(int id);

private:
    Storage *&findStorage(int id)
    {
        /* resize if needed, add a space margin */
        if (id >= _locals.size())
            _locals.resize(id + MIN_LOCALS);

        /* lookup by index */
        return _locals[id];
    }

/** Dynamic Thread-Local Storage
 *
 *  in addition to C++'s native `thread_local` which must be static,
 *  this implementation allows for instance-level thread-local storage but of course,
 *  it's a bit slower than native `thread_local`, and has a global count limit (which
 *  could be set by `Thread::MAX_LOCALS`)
 */

public:
    template <typename T>
    class Local final
    {
        int _id;
        size_t _reclaim;
        std::function<T *()> _init;
        std::function<void(T *)> _clean;

    private:
        static void defaultClean(T *object) { delete object; }

    public:
       ~Local() { Thread::releaseLocalId(_id); }
        Local(std::function<T *()> &&init) : Local(std::move(init), &defaultClean) {}
        Local(std::function<T *()> &&init, std::function<void(T *)> &&clean) :
            _id(Thread::allocLocalId(_reclaim)),
            _init(std::move(init)),
            _clean(std::move(clean))
        {
            /* check ID range */
            if (_id < 0)
                throw std::overflow_error("Too much dynamic thread locals");

            /* check initializer and finallizer */
            if (!_init || !_clean)
                throw std::invalid_argument("Invalid initializer or finallizer");
        }

    private:
        T *updateStorage(Storage *&locals) const
        {
            /* release old storage if any */
            if (locals == nullptr)
                locals = new Storage;
            else
                locals->destroy(locals->ptr);

            /* setup new storage */
            locals->ptr = _init();
            locals->reclaim = _reclaim;
            locals->destroy = [clean = std::move(_clean)](void *p)
            {
                if (clean != nullptr)
                    clean(reinterpret_cast<T *>(p));
            };

            /* convert to desired types */
            return reinterpret_cast<T *>(locals->ptr);
        }

    public:
        T &get(void) const
        {
            /* get the storage from current thread */
            Thread &thread = Thread::current();
            Storage *&locals = thread.findStorage(_id);

            /* storage not created, create it;
             * or there is values already in the local storage,
             * it could be from an old local that has been deleted */
            if (!locals || (locals->reclaim != _reclaim))
                return *updateStorage(locals);
            else
                return *reinterpret_cast<T *>(locals->ptr);
        }

    public:
        T &operator*(void) { return get(); }
        T *operator->(void) { return &get(); }

    public:
        const T &operator*(void) const { return get(); }
        const T *operator->(void) const { return &get(); }

    };
};
}

#endif /* REDSCRIPT_ENGINE_THREAD_H */
