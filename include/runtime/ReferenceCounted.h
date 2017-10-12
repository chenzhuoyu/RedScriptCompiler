#ifndef REDSCRIPT_RUNTIME_REFERENCECOUNTED_H
#define REDSCRIPT_RUNTIME_REFERENCECOUNTED_H

#include <new>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <climits>
#include <typeinfo>
#include <stdexcept>

#include "utils/Pointers.h"

namespace RedScript::Runtime
{
template <typename T>
class Reference final
{
    T *_object;

public:
   ~Reference() { unref(); }
    Reference(T *object = nullptr) : _object(object) { ref(); }

public:
    /* move constructor and copy constuctor */
    Reference(Reference<T> &&other) : _object(nullptr) { swap(other); }
    Reference(const Reference<T> &other) : _object(nullptr) { assign(other); }

public:
    /* up-cast constructor */
    template <typename U>
    Reference(const Reference<U> &other) : _object(nullptr)
    {
        /* compiler will complain for us if `U *` is not directly convertible to `T *` */
        assign(other);
    }

private:
    /* reference-borrowing constructor, internal use only */
    struct Tag {};
    friend class Object;
    Reference(T *object, Tag) : _object(object) { ref(); }

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
            if (_object->_isStatic)
                _object = nullptr;
            else
                Utils::Pointers::deleteAndSetNull(_object);
        }
    }

public:
    void swap(Reference<T> &other)
    {
        /* swap with other object */
        std::swap(_object, other._object);
    }

public:
    template <typename U>
    void assign(const Reference<U> &other)
    {
        other.ref();
        this->unref();
        _object = other._object;
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
            return Reference<U>(object, Tag());
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
    size_t refCount(void) const { return _object ? _object->_refCount : SIZE_T_MAX; }

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
    static Reference<T> refStatic(T &object)
    {
        if (!object._isStatic)
            throw std::invalid_argument("Object must be static");
        else
            return Reference<T>(&object, Tag());
    }
};

class ReferenceCounted
{
    template <typename>
    friend class Reference;

private:
    bool _isStatic;
    std::atomic_int32_t _refCount;

protected:
    virtual ~ReferenceCounted() {}
    explicit ReferenceCounted();

public:
    bool isStatic(void) const { return _isStatic; }
    int32_t refCount(void) const { return _refCount.load(); }

public:
    /* override `new` and `delete` operators to identify static and heap objects */
    static void *operator new(size_t size);
    static void  operator delete(void *self);

private:
    /* doesn't allow array allocations */
    static void *operator new[](size_t) = delete;
    static void  operator delete[](void *) = delete;

};
}

#endif /* REDSCRIPT_RUNTIME_REFERENCECOUNTED_H */
