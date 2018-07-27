#ifndef REDSCRIPT_RUNTIME_REFERENCECOUNTED_H
#define REDSCRIPT_RUNTIME_REFERENCECOUNTED_H

#include <new>
#include <cstdio>
#include <cstdint>
#include <climits>
#include <typeinfo>
#include <stdexcept>
#include <functional>
#include <type_traits>

#include "utils/Immovable.h"
#include "utils/NonCopyable.h"

namespace RedScript::Runtime
{
template <typename F> struct _IsStaticFlag : public std::false_type {};
template <typename F> struct _IsComparator : public std::false_type {};

template <>                       struct _IsStaticFlag<bool> : public std::true_type {};
template <typename T, typename U> struct _IsComparator<bool(T::*)(U *)> : public std::true_type {};

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCSimplifyInspection"
#pragma ide diagnostic ignored "NotImplementedFunctions"

template <typename T>
class _HasStaticMember
{
    struct _One {};
    struct _Two { char _[2]; };

private:
    template <typename U> static _One __testIsStatic(decltype(&U::_isStatic));
    template <typename U> static _Two __testIsStatic(...);

public:
    static constexpr bool value = (sizeof(__testIsStatic<T>(false)) == sizeof(_One));

};

template <typename T>
class _HasComparatorMethods
{
    struct _One {};
    struct _Two { char _[2]; };

private:
    template <typename U> static _One __testIsEquals(decltype(&U::isEquals));
    template <typename U> static _Two __testIsEquals(...);

private:
    template <typename U> static _One __testIsNotEquals(decltype(&U::isNotEquals));
    template <typename U> static _Two __testIsNotEquals(...);

public:
    static constexpr bool value =
        (sizeof(__testIsEquals<T>(nullptr)) == sizeof(_One)) &&
        (sizeof(__testIsNotEquals<T>(nullptr)) == sizeof(_One));
};

template <typename T, bool hasStaticFlag>
struct _HasStaticFlagImpl : public std::false_type {};

template <typename T, bool hasComparator>
struct _HasComparatorImpl : public std::false_type {};

template <typename T>
struct _HasStaticFlagImpl<T, true>
{
    typedef decltype(&T::_isStatic) StaticFlagType;
    static constexpr bool value = _IsStaticFlag<StaticFlagType>::value;
};

template <typename T>
struct _HasComparatorImpl<T, true>
{
    static constexpr bool value =
        _IsComparator<decltype(&T::isEquals)>::value &&
        _IsComparator<decltype(&T::isNotEquals)>::value;
};

template <typename T, bool hasStaticFlag>
struct _StaticFlagSetterImpl
{
    static inline void set(T &object, bool)
    {
        /* no static flags */
        /* do nothing here */
    }
};

template <typename T>
struct _StaticFlagSetterImpl<T, true>
{
    static inline void set(T &object, bool value)
    {
        /* set the static object flag */
        object._isStatic = value;
    }
};

template <typename T, typename U, bool hasComparator>
struct _ReferenceComparatorImpl
{
    static inline bool isEquals(T *self, U *other) { return self == other; }
    static inline bool isNotEquals(T *self, U *other) { return self != other; }
};

template <typename T, typename U>
struct _ReferenceComparatorImpl<T, U, true>
{
    static inline bool isEquals(T *self, U *other) { return self->isEquals(other); }
    static inline bool isNotEquals(T *self, U *other) { return self->isNotEquals(other); }
};

template <typename T> using _HasStaticFlag = _HasStaticFlagImpl<T, _HasStaticMember<T>::value>;
template <typename T> using _HasComparator = _HasComparatorImpl<T, _HasComparatorMethods<T>::value>;

template <typename T> using _StaticFlagSetter = _StaticFlagSetterImpl<T, _HasStaticFlag<T>::value>;
template <typename T, typename U> using _ReferenceComparator = _ReferenceComparatorImpl<T, U, _HasComparator<T>::value>;

#pragma clang diagnostic pop

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
    /* default constructor and null-value constructor */
    Reference() : Reference(nullptr) {}
    Reference(std::nullptr_t) : _object(nullptr), _isBorrowed(false) {}

public:
    /* move constructor and copy constuctor */
    Reference(Reference<T> &&other) : _object(nullptr), _isBorrowed(false) { swap(other); }
    Reference(const Reference<T> &other) : _object(nullptr), _isBorrowed(false) { assign(other); }

public:
    /* up-cast constructor */
    template <typename U>
    Reference(const Reference<U> &other) : _object(nullptr), _isBorrowed(false)
    {
        /* compiler will complain if `U *` is not directly convertible to `T *` */
        assign(other);
    }

private:
    template <typename>
    friend class Reference;
    friend class ReferenceCounted;

private:
    /* constructor for newly created objects, internal use only */
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
    inline void ref(void) const
    {
        if (_object)
            __sync_add_and_fetch(&(_object->_refCount), 1);
    }

private:
    inline void unref(void)
    {
        if (_object && !(__sync_sub_and_fetch(&(_object->_refCount), 1)))
        {
            T *temp = _object;
            _object = nullptr;
            delete temp;
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
            unref();

        /* copy objects, but no longer borrowed */
        _object = other._object;
        _isBorrowed = false;
    }

public:
    template <typename U>
    Reference<U> as(void) const
    {
        return Reference<U>(
            static_cast<U *>(_object),
            typename Reference<U>::TagBorrowed()
        );
    }

public:
    T &operator*(void) { return *_object; }
    T *operator->(void) { return _object; }

public:
    T *get(void) { return _object; }
    bool isStatic(void) const { return _object ? _object->isStatic() : true; }
    size_t refCount(void) const { return _object ? _object->refCount() : SIZE_MAX; }

public:
    bool isNull(void) const { return _object == nullptr; }
    bool isNotNull(void) const { return _object != nullptr; }

public:
    template <typename U> bool isIdenticalWith(const Reference<U> &other) const { return _object == other._object; }
    template <typename U> bool isNotIdenticalWith(const Reference<U> &other) const { return _object != other._object; }

public:
    operator T *(void) { return _object; }
    operator bool(void) const { return _object != nullptr; }

public:
    bool operator!=(std::nullptr_t) const { return _object != nullptr; }
    bool operator==(std::nullptr_t) const { return _object == nullptr; }

public:
    template <typename U>
    bool operator==(Reference<U> other) const
    {
        if (_object == other._object)
            return true;
        else if (!_object || !(other._object))
            return false;
        else
            return _ReferenceComparator<T, U>::isEquals(_object, other._object);
    }

public:
    template <typename U>
    bool operator!=(Reference<U> other) const
    {
        if (_object == other._object)
            return false;
        else if (!_object || !(other._object))
            return true;
        else
            return _ReferenceComparator<T, U>::isNotEquals(_object, other._object);
    }

public:
    static inline Reference<T> retain(T *object) { return Reference<T>(object, TagChecked()); }
    static inline Reference<T> borrow(T *object) { return Reference<T>(object, TagBorrowed()); }

public:
    static inline Reference<T> refStatic(T &object)
    {
        _StaticFlagSetter<T>::set(object, true);
        return Reference<T>(&object, TagBorrowed());
    }

public:
    template <typename ... Args>
    static inline Reference<T> newObject(Args && ... args)
    {
        auto object = new T(std::forward<Args>(args) ...);
        return Reference<T>(object, TagNew());
    }
};

class ReferenceCounted : public Utils::Immovable, public Utils::NonCopyable
{
    template <typename>
    friend class Reference;

private:
    bool _isStatic;
    int32_t _refCount;

protected:
    virtual ~ReferenceCounted() = default;
    explicit ReferenceCounted() : _isStatic(false), _refCount(1) {}

public:
    typedef std::function<void(ReferenceCounted *)> VisitFunction;

public:
    bool isStatic(void) const { return _isStatic; }
    int32_t refCount(void) const { return _refCount; }

public:
    void track(void) const;
    void untrack(void) const;
    bool isTracked(void) const;

public:
    virtual void referenceClear(void) {}
    virtual void referenceTraverse(VisitFunction visit) {}

protected:
    /* heap objects can only be created by `Reference<T>` */
    static void *operator new(size_t size);
    static void  operator delete(void *self);

private:
    /* doesn't allow array allocations */
    static void *operator new[](size_t) = delete;
    static void  operator delete[](void *) = delete;

};
}

#endif /* REDSCRIPT_RUNTIME_REFERENCECOUNTED_H */
