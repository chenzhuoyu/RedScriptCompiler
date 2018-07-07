#ifndef REDSCRIPT_RUNTIME_OBJECT_H
#define REDSCRIPT_RUNTIME_OBJECT_H

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include "runtime/ReferenceCounted.h"

namespace RedScript::Runtime
{
class Type;
class Object;
typedef Reference<Type> TypeRef;
typedef Reference<Object> ObjectRef;
typedef std::vector<std::string> StringList;
typedef std::unordered_map<std::string, ObjectRef> Dict;

/* the very first class */
extern TypeRef TypeObject;

/* simple object */
class Object : public ReferenceCounted
{
    Dict _dict;
    TypeRef _type;

private:
    template <typename> friend class Reference;
    template <typename> friend class _HasComparatorMethods;
    template <typename, bool> friend struct _HasComparatorImpl;
    template <typename, typename, bool> friend struct _ReferenceComparatorImpl;

public:
    virtual ~Object() { untrack(); }
    explicit Object(TypeRef type) : _type(type) {}

private:
    /* used by `_HasComparator<T>` and `_ReferenceComparator<T, U>` to perform equality test */
    bool isEquals(Object *other);
    bool isNotEquals(Object *other);

public:
    Dict &dict(void) { return _dict; }
    TypeRef type(void) { return _type; }
    ObjectRef self(void) { return ObjectRef::borrow(this); }

public:
    bool isInstanceOf(TypeRef type) { return _type.isIdenticalWith(type); }
    bool isNotInstanceOf(TypeRef type) { return !(_type.isIdenticalWith(type)); }

public:
    static void shutdown(void) {}
    static void initialize(void);

public:
    template <typename T, typename ... Args>
    static inline Reference<T> newObject(Args && ... args)
    {
        /* just a shortcut function */
        return Reference<T>::newObject(std::forward<Args>(args) ...);
    }
};

/* class object */
class Type : public Object
{
    TypeRef _super;
    std::string _name;

public:
    explicit Type(const std::string &name) : Type(name, TypeObject) {}
    explicit Type(const std::string &name, TypeRef super) : Object(TypeObject), _name(name), _super(super) {}

public:
    TypeRef super(void) { return _super; }
    std::string &name(void) { return _name; }

private:
    ObjectRef applyUnary(const char *name, ObjectRef self);
    ObjectRef applyBinary(const char *name, ObjectRef self, ObjectRef other, const char *alternative = nullptr);
    ObjectRef applyTernary(const char *name, ObjectRef self, ObjectRef second, ObjectRef third);

/*** Object Protocol ***/

public:
    virtual uint64_t    objectHash(ObjectRef self);
    virtual StringList  objectDir (ObjectRef self);
    virtual std::string objectStr (ObjectRef self) { return objectRepr(self); }
    virtual std::string objectRepr(ObjectRef self);

public:
    virtual bool objectIsTrue(ObjectRef self) { return true; }
    virtual bool objectIsSubclassOf(ObjectRef self, TypeRef type);
    virtual bool objectIsInstanceOf(ObjectRef self, TypeRef type) { return objectIsSubclassOf(self->type(), type); }

private:
    enum class DescriptorType { Proxy, Object, NotADescriptor };
    DescriptorType resolveDescriptor(ObjectRef obj, ObjectRef &getter, ObjectRef &setter, ObjectRef &deleter);

public:
    virtual void      objectDelAttr(ObjectRef self, const std::string &name);
    virtual ObjectRef objectGetAttr(ObjectRef self, const std::string &name);
    virtual void      objectSetAttr(ObjectRef self, const std::string &name, ObjectRef value);
    virtual void      objectDefineAttr(ObjectRef self, const std::string &name, ObjectRef value);

public:
    virtual ObjectRef objectInvoke(ObjectRef self, ObjectRef args, ObjectRef kwargs);

/*** Boolean Protocol ***/

public:
    virtual ObjectRef boolOr (ObjectRef self, ObjectRef other);
    virtual ObjectRef boolAnd(ObjectRef self, ObjectRef other);
    virtual ObjectRef boolNot(ObjectRef self);

/*** Numeric Protocol ***/

public:
    virtual ObjectRef numericPos(ObjectRef self) { return applyUnary("__pos__", self); }
    virtual ObjectRef numericNeg(ObjectRef self) { return applyUnary("__neg__", self); }

public:
    virtual ObjectRef numericAdd  (ObjectRef self, ObjectRef other) { return applyBinary("__add__", self, other); }
    virtual ObjectRef numericSub  (ObjectRef self, ObjectRef other) { return applyBinary("__sub__", self, other); }
    virtual ObjectRef numericMul  (ObjectRef self, ObjectRef other) { return applyBinary("__mul__", self, other); }
    virtual ObjectRef numericDiv  (ObjectRef self, ObjectRef other) { return applyBinary("__div__", self, other); }
    virtual ObjectRef numericMod  (ObjectRef self, ObjectRef other) { return applyBinary("__mod__", self, other); }
    virtual ObjectRef numericPower(ObjectRef self, ObjectRef other) { return applyBinary("__pow__", self, other); }

public:
    virtual ObjectRef numericOr (ObjectRef self, ObjectRef other) { return applyBinary("__or__" , self, other); }
    virtual ObjectRef numericAnd(ObjectRef self, ObjectRef other) { return applyBinary("__and__", self, other); }
    virtual ObjectRef numericXor(ObjectRef self, ObjectRef other) { return applyBinary("__xor__", self, other); }
    virtual ObjectRef numericNot(ObjectRef self)                  { return applyUnary ("__not__", self       ); }

public:
    virtual ObjectRef numericLShift(ObjectRef self, ObjectRef other) { return applyBinary("__lshift__", self, other); }
    virtual ObjectRef numericRShift(ObjectRef self, ObjectRef other) { return applyBinary("__rshift__", self, other); }

public:
    virtual ObjectRef numericIncAdd  (ObjectRef self, ObjectRef other) { return applyBinary("__inc_add__", self, other, "__add__"); }
    virtual ObjectRef numericIncSub  (ObjectRef self, ObjectRef other) { return applyBinary("__inc_sub__", self, other, "__sub__"); }
    virtual ObjectRef numericIncMul  (ObjectRef self, ObjectRef other) { return applyBinary("__inc_mul__", self, other, "__mul__"); }
    virtual ObjectRef numericIncDiv  (ObjectRef self, ObjectRef other) { return applyBinary("__inc_div__", self, other, "__div__"); }
    virtual ObjectRef numericIncMod  (ObjectRef self, ObjectRef other) { return applyBinary("__inc_mod__", self, other, "__mod__"); }
    virtual ObjectRef numericIncPower(ObjectRef self, ObjectRef other) { return applyBinary("__inc_pow__", self, other, "__pow__"); }

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
    virtual void      sequenceDelItem(ObjectRef self, ObjectRef other)                   {        applyBinary ("__delitem__", self, other        ); }
    virtual ObjectRef sequenceGetItem(ObjectRef self, ObjectRef other)                   { return applyBinary ("__getitem__", self, other        ); }
    virtual void      sequenceSetItem(ObjectRef self, ObjectRef second, ObjectRef third) {        applyTernary("__setitem__", self, second, third); }

/*** Comparable Protocol ***/

public:
    virtual ObjectRef comparableEq(ObjectRef self, ObjectRef other);
    virtual ObjectRef comparableLt(ObjectRef self, ObjectRef other) { return applyBinary("__lt__", self, other); }
    virtual ObjectRef comparableGt(ObjectRef self, ObjectRef other) { return applyBinary("__gt__", self, other); }

public:
    virtual ObjectRef comparableNeq(ObjectRef self, ObjectRef other);
    virtual ObjectRef comparableLeq(ObjectRef self, ObjectRef other) { return applyBinary("__leq__", self, other); }
    virtual ObjectRef comparableGeq(ObjectRef self, ObjectRef other) { return applyBinary("__geq__", self, other); }

public:
    virtual ObjectRef comparableCompare(ObjectRef self, ObjectRef other) { return applyBinary("__compare__", self, other); }
    virtual ObjectRef comparableContains(ObjectRef self, ObjectRef other) { return applyBinary("__contains__", self, other); }

};
}

/* hash function for STL
 * required by unordered data structures */
namespace std
{
template <>
struct hash<RedScript::Runtime::ObjectRef>
{
    size_t operator()(RedScript::Runtime::ObjectRef other) const
    {
        /* use object system hash function */
        std::hash<uint64_t> hash;
        return hash(other->type()->objectHash(other));
    }
};
}

#endif /* REDSCRIPT_RUNTIME_OBJECT_H */
