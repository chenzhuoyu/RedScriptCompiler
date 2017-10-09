#ifndef REDSCRIPT_RUNTIME_OBJECT_H
#define REDSCRIPT_RUNTIME_OBJECT_H

#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <typeinfo>
#include <stdexcept>
#include <functional>
#include <unordered_map>

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
            __sync_add_and_fetch(&(_object->_refCount), 1);
    }

private:
    void unref()
    {
        if (_object && !__sync_sub_and_fetch(&(_object->_refCount), 1))
        {
            if (_object->_isStatic)
                _object = nullptr;
            else
                Pointers::deleteAndSetNull(_object);
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

class Type;
class Object;
typedef Reference<Type> TypeRef;
typedef Reference<Object> ObjectRef;
typedef std::vector<std::string> StringList;
typedef std::unordered_map<std::string, ObjectRef> Elements;

/* the very first class */
extern TypeRef MetaType;

/* simple object */
class Object
{
    bool _isStatic = true;
    size_t _refCount = 0;

private:
    TypeRef _type;
    Elements _dict;

private:
    template <typename>
    friend class Reference;
    friend class MetaClassInit;

public:
    virtual ~Object();
    explicit Object(TypeRef type);

public:
    TypeRef type(void) { return _type; }
    ObjectRef self(void) { return Reference<Object>(this); }

public:
    bool isStatic(void) const { return _isStatic; }
    size_t refCount(void) const { return _refCount; }

public:
    void *operator new(size_t size)
    {
        /* check for minimun required size */
        if (size < sizeof(Object))
            throw std::bad_alloc();

        /* allocate new class */
        void *mem = malloc(size);

        /* check for allocation */
        if (!mem)
            throw std::bad_alloc();

        /* mark as dynamic created object */
        static_cast<Object *>(mem)->_isStatic = false;
        return mem;
    }

public:
    void operator delete(void *self)
    {
        /* just free the object directly */
        free(self);
    }
};

/* class object */
class Type : public Object
{
    TypeRef _super;

public:
    explicit Type() : Type(MetaType) {}
    explicit Type(TypeRef super) : Object(MetaType), _super(super) {}

public:
    TypeRef super(void) const { return _super; }

private:
    ObjectRef applyUnary(const char *name, ObjectRef self);
    ObjectRef applyBinary(const char *name, ObjectRef self, ObjectRef other, const char *alternative = nullptr);
    ObjectRef applyTernary(const char *name, ObjectRef self, ObjectRef second, ObjectRef third);

/*** Object Protocol ***/

public:
    typedef std::function<void(ObjectRef)> VisitFunction;

public:
    virtual void objectInit    (ObjectRef self);
    virtual void objectClear   (ObjectRef self);
    virtual void objectDestroy (ObjectRef self);
    virtual void objectTraverse(ObjectRef self, VisitFunction visit);

public:
    virtual uint64_t    objectHash(ObjectRef self);
    virtual StringList  objectDir (ObjectRef self);
    virtual std::string objectStr (ObjectRef self);
    virtual std::string objectRepr(ObjectRef self);

public:
    virtual ObjectRef objectDelAttr(ObjectRef self, const std::string &name);
    virtual ObjectRef objectGetAttr(ObjectRef self, const std::string &name);
    virtual ObjectRef objectSetAttr(ObjectRef self, const std::string &name, ObjectRef value);

public:
    virtual ObjectRef objectInvoke(ObjectRef self, const std::vector<ObjectRef> &args);

/*** Boolean Protocol ***/

public:
    virtual ObjectRef boolOr (ObjectRef self, ObjectRef other) { return applyBinary("__bool_or__" , self, other); }
    virtual ObjectRef boolAnd(ObjectRef self, ObjectRef other) { return applyBinary("__bool_and__", self, other); }
    virtual ObjectRef boolXor(ObjectRef self, ObjectRef other) { return applyBinary("__bool_xor__", self, other); }
    virtual ObjectRef boolNot(ObjectRef self)                  { return applyUnary ("__bool_not__", self       ); }

/*** Numeric Protocol ***/

public:
    virtual ObjectRef numericPos(ObjectRef self) { return applyUnary("__pos__", self); }
    virtual ObjectRef numericNeg(ObjectRef self) { return applyUnary("__neg__", self); }

public:
    virtual ObjectRef numericAdd(ObjectRef self, ObjectRef other) { return applyBinary("__add__", self, other); }
    virtual ObjectRef numericSub(ObjectRef self, ObjectRef other) { return applyBinary("__sub__", self, other); }
    virtual ObjectRef numericMul(ObjectRef self, ObjectRef other) { return applyBinary("__mul__", self, other); }
    virtual ObjectRef numericDiv(ObjectRef self, ObjectRef other) { return applyBinary("__div__", self, other); }
    virtual ObjectRef numericMod(ObjectRef self, ObjectRef other) { return applyBinary("__mod__", self, other); }
    virtual ObjectRef numericPow(ObjectRef self, ObjectRef other) { return applyBinary("__pow__", self, other); }

public:
    virtual ObjectRef numericOr (ObjectRef self, ObjectRef other) { return applyBinary("__or__" , self, other); }
    virtual ObjectRef numericAnd(ObjectRef self, ObjectRef other) { return applyBinary("__and__", self, other); }
    virtual ObjectRef numericXor(ObjectRef self, ObjectRef other) { return applyBinary("__xor__", self, other); }
    virtual ObjectRef numericNot(ObjectRef self)                  { return applyUnary ("__not__", self       ); }

public:
    virtual ObjectRef numericLShift(ObjectRef self, ObjectRef other) { return applyBinary("__lshift__", self, other); }
    virtual ObjectRef numericRShift(ObjectRef self, ObjectRef other) { return applyBinary("__rshift__", self, other); }

public:
    virtual ObjectRef numericIncAdd(ObjectRef self, ObjectRef other) { return applyBinary("__inc_add__", self, other, "__add__"); }
    virtual ObjectRef numericIncSub(ObjectRef self, ObjectRef other) { return applyBinary("__inc_sub__", self, other, "__sub__"); }
    virtual ObjectRef numericIncMul(ObjectRef self, ObjectRef other) { return applyBinary("__inc_mul__", self, other, "__mul__"); }
    virtual ObjectRef numericIncDiv(ObjectRef self, ObjectRef other) { return applyBinary("__inc_div__", self, other, "__div__"); }
    virtual ObjectRef numericIncMod(ObjectRef self, ObjectRef other) { return applyBinary("__inc_mod__", self, other, "__mod__"); }
    virtual ObjectRef numericIncPow(ObjectRef self, ObjectRef other) { return applyBinary("__inc_pow__", self, other, "__pow__"); }

public:
    virtual ObjectRef numericIncOr (ObjectRef self, ObjectRef other) { return applyBinary("__inc_or__" , self, other, "__or__" ); }
    virtual ObjectRef numericIncAnd(ObjectRef self, ObjectRef other) { return applyBinary("__inc_and__", self, other, "__and__"); }
    virtual ObjectRef numericIncXor(ObjectRef self, ObjectRef other) { return applyBinary("__inc_xor__", self, other, "__xor__"); }

public:
    virtual ObjectRef numericIncLShift(ObjectRef self, ObjectRef other) { return applyBinary("__inc_lshift__", self, other, "__lshift__"); }
    virtual ObjectRef numericIncRShift(ObjectRef self, ObjectRef other) { return applyBinary("__inc_rshift__", self, other, "__rshift__"); }

/*** Iterable Protocol ***/

public:
    virtual ObjectRef iterableIter(ObjectRef self) { return applyUnary("__iter__", self); }
    virtual ObjectRef iterableNext(ObjectRef self) { return applyUnary("__next__", self); }

/*** Sequence Protocol ***/

public:
    virtual ObjectRef sequenceLen    (ObjectRef self)                                    { return applyUnary  ("__len__"    , self               ); }
    virtual ObjectRef sequenceDelItem(ObjectRef self, ObjectRef other)                   { return applyBinary ("__delitem__", self, other        ); }
    virtual ObjectRef sequenceGetItem(ObjectRef self, ObjectRef other)                   { return applyBinary ("__getitem__", self, other        ); }
    virtual ObjectRef sequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third) { return applyTernary("__setitem__", self, second, third); }

/*** Comparable Protocol ***/

public:
    virtual ObjectRef comparableEq(ObjectRef self, ObjectRef other);
    virtual ObjectRef comparableLe(ObjectRef self, ObjectRef other) { return applyBinary("__le__", self, other); }
    virtual ObjectRef comparableGe(ObjectRef self, ObjectRef other) { return applyBinary("__ge__", self, other); }

public:
    virtual ObjectRef comparableNeq(ObjectRef self, ObjectRef other);
    virtual ObjectRef comparableLeq(ObjectRef self, ObjectRef other) { return applyBinary("__leq__", self, other); }
    virtual ObjectRef comparableGeq(ObjectRef self, ObjectRef other) { return applyBinary("__geq__", self, other); }

public:
    virtual ObjectRef comparableCompare(ObjectRef self, ObjectRef other) { return applyBinary("__compare__", self, other); }

};
}

#endif /* REDSCRIPT_RUNTIME_OBJECT_H */
