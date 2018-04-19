#ifndef REDSCRIPT_RUNTIME_REFERENCECOUNTED_H
#define REDSCRIPT_RUNTIME_REFERENCECOUNTED_H

#include <new>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <climits>
#include <typeinfo>
#include <stdexcept>

#include "utils/Immovable.h"
#include "utils/NonCopyable.h"

namespace RedScript::Runtime
{
template <typename T>
class Reference final
{
    T *_object;
    bool _isBorrowed;

public:
    ~Reference()
    {
        /* don't clean borrowed refs */
        if (!_isBorrowed)
            unref();
    }

public:
    Reference(T *object = nullptr) : _object(object), _isBorrowed(false)
    {
        /* cannot reference static object using this constructor */
        if (object && object->_isStatic)
            throw std::invalid_argument("Static objects must be referenced by `refStatic()`");

        /* add a reference */
        ref();
    }

public:
    /* move constructor and copy constuctor */
    Reference(Reference<T> &&other) : _object(nullptr), _isBorrowed(false) { swap(other); }
    Reference(const Reference<T> &other) : _object(nullptr), _isBorrowed(false) { assign(other); }

public:
    /* up-cast constructor */
    template <typename U>
    Reference(const Reference<U> &other) : _object(nullptr), _isBorrowed(false)
    {
        /* compiler will complain for us if `U *` is not directly convertible to `T *` */
        assign(other);
    }

private:
    template <typename>
    friend class Reference;
    friend class ReferenceCounted;

private:
    /* reference-borrowing constructor for newly created objects, internal use only */
    struct TagNew {};
    Reference(T *object, TagNew) : _object(object), _isBorrowed(false) {}

private:
    /* checked object referencing constructor, internal use only */
    struct TagChecked {};
    Reference(T *object, TagChecked) : _object(object), _isBorrowed(false) { ref(); }

private:
    /* object reference-borrowing constructor, internal use only */
    struct TagBorrowed {};
    Reference(T *object, TagBorrowed) : _object(object), _isBorrowed(true) {}

public:
    Reference<T> &operator=(Reference<T> &&other) { swap(other); return *this; }
    Reference<T> &operator=(const Reference<T> &other) { assign(other); return *this; }

private:
    void ref(void) const
    {
        if (_object)
            _object->_refCount++;
    }

private:
    void unref()
    {
        if (_object && !(--_object->_refCount))
        {
            /* reference and null the object first */
            T *object = _object;
            _object = nullptr;

            /* if the object is dynamically allocated, then reclaim the object */
            if (!(object->_isStatic))
                delete object;
        }
    }

public:
    void swap(Reference<T> &other)
    {
        std::swap(_object, other._object);
        std::swap(_isBorrowed, other._isBorrowed);
    }

public:
    template <typename U>
    void assign(const Reference<U> &other)
    {
        /* add reference first */
        other.ref();

        /* don't clean borrowed refs */
        if (!_isBorrowed)
            this->unref();

        /* transfer objects */
        _object = other._object;
        _isBorrowed = other._isBorrowed;
    }

public:
    template <typename U>
    Reference<U> as(void) const
    {
        /* use `dynamic_cast` to perform down-cast or side-cast */
        U *object = dynamic_cast<U *>(_object);

        /* check for cast result */
        if (!object && _object)
            throw std::bad_cast();
        else
            return Reference<U>(object, TagChecked());
    }

public:
    T &operator*(void) { return *_object; }
    T *operator->(void) { return _object; }

public:
    const T &operator*(void) const { return *_object; }
    const T *operator->(void) const { return _object; }

public:
    bool isNull(void) const { return !_object; }
    bool isStatic(void) const { return _object ? _object->_isStatic : true; }
    size_t refCount(void) const { return _object ? _object->_refCount : SIZE_MAX; }

public:
    operator T *(void) { return _object; }
    operator bool(void) const { return _object != nullptr; }
    operator const T *(void) const { return _object; }

public:
    bool operator!=(std::nullptr_t) const { return _object == nullptr; }
    bool operator==(std::nullptr_t) const { return _object != nullptr; }

public:
    template <typename U> bool operator==(const Reference<U> &other) const { return _object == other._object; }
    template <typename U> bool operator!=(const Reference<U> &other) const { return _object != other._object; }

public:
    template <typename U>
    static inline Reference<T> borrow(U *object)
    {
        /* use `dynamic_cast` to perform down-cast or side-cast */
        T *newObject = dynamic_cast<T *>(object);

        /* borrow a reference from `object`, if viable */
        if (!newObject)
            throw std::bad_cast();
        else
            return Reference<T>(newObject, TagBorrowed());
    }

public:
    static inline Reference<T> refStatic(T &object)
    {
        if (!object._isStatic)
            throw std::invalid_argument("Object must be static");
        else
            return Reference<T>(&object, TagChecked());
    }

public:
    template <typename ... Args>
    static inline Reference<T> newObject(Args && ... args)
    {
        T *object = new T(std::forward<Args>(args) ...);
        return Reference<T>(object, TagNew());
    }
};

class ReferenceCounted : public Utils::Immovable, public Utils::NonCopyable
{
    template <typename>
    friend class Reference;

private:
    bool _isStatic;
    std::atomic_int _refCount;

protected:
    virtual ~ReferenceCounted() {}
    explicit ReferenceCounted();

public:
    bool isStatic(void) const { return _isStatic; }
    int32_t refCount(void) const { return _refCount.load(); }

public:
    void track(void) const;
    void untrack(void) const;
    bool isTracked(void) const;

protected:
    /* override `new` and `delete` operators to identify static
     * and heap objects, and can only be created by `Reference<T>` */
    static void *operator new(size_t size);
    static void  operator delete(void *self);

private:
    /* doesn't allow array allocations */
    static void *operator new[](size_t) = delete;
    static void  operator delete[](void *) = delete;

};
}

#endif /* REDSCRIPT_RUNTIME_REFERENCECOUNTED_H */
